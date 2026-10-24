// ============================================================
//  PyBank ATM Simulator
//  A beginner-friendly OOP mini project in C++11.
//
//  Compile (MinGW / g++):
//      g++ -std=c++11 -Wall -Wextra -pedantic main.cpp -o pybank_atm.exe
//
//  Run:
//      pybank_atm.exe        (Windows)
//      ./pybank_atm          (Linux / macOS)
//
//  This is an EDUCATIONAL simulator. It must never be used with
//  real banking credentials or real money.
// ============================================================

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace std;

// ============================================================
//  CONFIGURATION  (safe to change for a demonstration)
// ============================================================

const int MAX_LOGIN_ATTEMPTS = 3;  // Maximum consecutive wrong logins
const long long DAILY_WITHDRAWAL_LIMIT_PAISE = 25000LL * 100;  // Rs. 25,000
// Set DAILY_WITHDRAWAL_LIMIT_PAISE to 0 to disable the daily limit.
// Transfers are not counted as cash withdrawals.

const int MIN_ACCOUNT_NUMBER_LENGTH = 6;
const int MAX_ACCOUNT_NUMBER_LENGTH = 12;

const string ACCOUNTS_FILE = "accounts.csv";
const string TRANSACTIONS_FILE = "transactions.csv";

const string ACCOUNTS_HEADER =
    "account_no,name,pin_salt,pin_hash,balance_paise,created_at";
const string TRANSACTIONS_HEADER =
    "account_no,date_time,type,amount_paise,balance_paise,reference,note";

// ============================================================
//  SMALL INPUT / TEXT UTILITIES
// ============================================================

string trim(const string& text)
{
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

string toLowerText(const string& text)
{
    string result = text;
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<char>(
            tolower(static_cast<unsigned char>(result[i])));
    }
    return result;
}

void closeProgramBecauseInputEnded()
{
    // Every completed operation is saved immediately, so exiting here is safe.
    cout << "\nInput stream closed. Exiting PyBank ATM safely.\n";
    exit(EXIT_SUCCESS);
}

string readLine(const string& prompt)
{
    cout << prompt;
    cout.flush();
    string line;
    if (!getline(cin, line)) {
        closeProgramBecauseInputEnded();
    }
    return trim(line);
}

void pauseScreen()
{
    cout << "\nPress Enter to continue...";
    cout.flush();
    string ignored;
    if (!getline(cin, ignored)) {
        closeProgramBecauseInputEnded();
    }
}

// Returns true when stdin is a real terminal. When input is redirected
// (for automated tests), the PIN is read as a normal line so that tests
// can supply input. Interactive users always get masked PIN input.
bool inputIsInteractive()
{
#ifdef _WIN32
    return _isatty(0) != 0;  // 0 is the standard-input file descriptor
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

string readPassword(const string& prompt)
{
    cout << prompt;
    cout.flush();

#ifdef _WIN32
    if (inputIsInteractive()) {
        string password;
        while (true) {
            int ch = _getch();
            if (ch == '\r' || ch == '\n') {
                break;
            }
            if (ch == 8) {  // Backspace
                if (!password.empty()) {
                    password.erase(password.size() - 1);
                    cout << "\b \b";
                }
            } else if (ch >= 32 && ch <= 126) {
                password += static_cast<char>(ch);
                cout << '*';
            }
        }
        cout << "\n";
        return password;
    }
#else
    if (isatty(STDIN_FILENO)) {
        termios oldSettings;
        termios newSettings;
        if (tcgetattr(STDIN_FILENO, &oldSettings) == 0) {
            newSettings = oldSettings;
            newSettings.c_lflag &= ~static_cast<tcflag_t>(ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
        }

        string password;
        bool received = static_cast<bool>(getline(cin, password));

        if (tcgetattr(STDIN_FILENO, &oldSettings) == 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
        }
        cout << "\n";
        if (!received) {
            closeProgramBecauseInputEnded();
        }
        return trim(password);
    }
#endif

    string line;
    if (!getline(cin, line)) {
        closeProgramBecauseInputEnded();
    }
    cout << "\n";  // Keep redirected/test output tidy; no PIN is echoed.
    return trim(line);
}

bool isAllDigits(const string& text)
{
    if (text.empty()) {
        return false;
    }
    for (size_t i = 0; i < text.size(); ++i) {
        if (!isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
    }
    return true;
}

// Parses a non-negative whole number without using atoi()/stoi(),
// so that we can detect empty input and overflow.
bool parseNonNegativeInteger(const string& text, long long& value)
{
    string number = trim(text);
    if (!isAllDigits(number)) {
        return false;
    }

    long long result = 0;
    for (size_t i = 0; i < number.size(); ++i) {
        int digit = number[i] - '0';
        if (result > (LLONG_MAX - digit) / 10) {
            return false;  // Too large for long long
        }
        result = result * 10 + digit;
    }
    value = result;
    return true;
}

// Converts rupee text such as "1250", "1250.5" or "1250.50" into
// integer paise. No floating-point number is ever used for money.
bool parseRupeesToPaise(const string& text, long long& paise)
{
    string amount = trim(text);
    if (amount.empty()) {
        return false;
    }

    size_t dotPosition = amount.find('.');
    if (dotPosition != string::npos &&
        amount.find('.', dotPosition + 1) != string::npos) {
        return false;  // More than one decimal point
    }

    string rupeesPart =
        (dotPosition == string::npos) ? amount : amount.substr(0, dotPosition);
    string paisePart =
        (dotPosition == string::npos) ? "" : amount.substr(dotPosition + 1);

    if (!isAllDigits(rupeesPart)) {
        return false;  // Rejects signs, letters, commas and spaces
    }
    if (dotPosition != string::npos) {
        if (paisePart.empty() || paisePart.size() > 2 ||
            !isAllDigits(paisePart)) {
            return false;
        }
    }

    unsigned long long rupees = 0;
    for (size_t i = 0; i < rupeesPart.size(); ++i) {
        unsigned long long digit =
            static_cast<unsigned long long>(rupeesPart[i] - '0');
        if (rupees > (numeric_limits<unsigned long long>::max() - digit) / 10) {
            return false;
        }
        rupees = rupees * 10 + digit;
    }

    unsigned long long fraction = 0;
    if (paisePart.size() == 1) {
        fraction = static_cast<unsigned long long>(paisePart[0] - '0') * 10;
    } else if (paisePart.size() == 2) {
        fraction = static_cast<unsigned long long>(
                       (paisePart[0] - '0') * 10 + (paisePart[1] - '0'));
    }

    unsigned long long maxPaise = static_cast<unsigned long long>(LLONG_MAX);
    if (rupees > (maxPaise - fraction) / 100) {
        return false;  // Overflow
    }

    paise = static_cast<long long>(rupees * 100 + fraction);
    return true;
}

string groupIndianDigits(unsigned long long rupees)
{
    string digits = to_string(rupees);
    string grouped;

    // Indian grouping: 1,23,456 (first comma after 3 digits from the right,
    // then every 2 digits).
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        grouped.insert(grouped.begin(), digits[static_cast<size_t>(i)]);
        ++count;
        if (i > 0 &&
            (count == 3 || (count > 3 && (count - 3) % 2 == 0))) {
            grouped.insert(grouped.begin(), ',');
        }
    }
    return grouped;
}

string formatMoney(long long paise)
{
    bool negative = paise < 0;
    unsigned long long absolutePaise;
    if (negative) {
        absolutePaise =
            static_cast<unsigned long long>(-(paise + 1)) + 1ULL;
    } else {
        absolutePaise = static_cast<unsigned long long>(paise);
    }

    unsigned long long rupees = absolutePaise / 100;
    unsigned int fraction = static_cast<unsigned int>(absolutePaise % 100);

    ostringstream output;
    if (negative) {
        output << '-';
    }
    output << "Rs. " << groupIndianDigits(rupees) << '.'
           << setw(2) << setfill('0') << fraction;
    return output.str();
}

string currentDateTime()
{
    time_t now = time(NULL);
    tm* localTime = localtime(&now);
    if (localTime == NULL) {
        return "1970-01-01 00:00:00";
    }

    char buffer[32];
    if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime) == 0) {
        return "1970-01-01 00:00:00";
    }
    return string(buffer);
}

string todayDate()
{
    return currentDateTime().substr(0, 10);
}

// ============================================================
//  ACCOUNT NUMBER, PIN HASHING AND REFERENCE HELPERS
// ============================================================

mt19937& randomGenerator()
{
    static random_device device;
    static mt19937 generator(static_cast<unsigned int>(
        device() ^ static_cast<unsigned int>(time(NULL))));
    return generator;
}

string randomHexString(size_t numberOfDigits)
{
    static const char hexDigits[] = "0123456789abcdef";
    uniform_int_distribution<int> distribution(0, 15);
    string result;
    result.reserve(numberOfDigits);
    for (size_t i = 0; i < numberOfDigits; ++i) {
        result += hexDigits[distribution(randomGenerator())];
    }
    return result;
}

string generateSaltValue()
{
    return randomHexString(32);  // 32 hex characters = 16 random bytes
}

// FNV-1a is a simple deterministic hash, suitable for this classroom
// demonstration. It is NOT a production password/PIN hashing algorithm.
string calculatePinHash(const string& salt, const string& pin)
{
    const unsigned long long FNV_OFFSET_BASIS = 14695981039346656037ULL;
    const unsigned long long FNV_PRIME = 1099511628211ULL;

    string material = salt + ":" + pin;
    unsigned long long hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < material.size(); ++i) {
        hash ^= static_cast<unsigned char>(material[i]);
        hash *= FNV_PRIME;
    }

    static const char hexDigits[] = "0123456789abcdef";
    string result;
    result.reserve(16);
    for (int shift = 60; shift >= 0; shift -= 4) {
        result += hexDigits[(hash >> shift) & 0x0FULL];
    }
    return result;
}

bool isHexString(const string& text)
{
    if (text.empty()) {
        return false;
    }
    for (size_t i = 0; i < text.size(); ++i) {
        if (!isxdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
    }
    return true;
}

string generateTenDigitAccountNumber()
{
    uniform_int_distribution<int> firstDigit(1, 9);
    uniform_int_distribution<int> otherDigit(0, 9);

    string number;
    number += static_cast<char>('0' + firstDigit(randomGenerator()));
    for (int i = 0; i < 9; ++i) {
        number += static_cast<char>('0' + otherDigit(randomGenerator()));
    }
    return number;
}

string generateTransactionReference()
{
    static unsigned long long counter = 0;
    ++counter;

    string stamp = currentDateTime();
    string compactStamp;
    for (size_t i = 0; i < stamp.size(); ++i) {
        if (isdigit(static_cast<unsigned char>(stamp[i]))) {
            compactStamp += stamp[i];
        }
    }

    ostringstream reference;
    reference << "TXN-" << compactStamp << '-'
              << setw(6) << setfill('0') << (counter % 1000000ULL);
    return reference.str();
}

// ============================================================
//  VALIDATION HELPERS
// ============================================================

bool isValidAccountNumber(const string& accountNumber)
{
    if (accountNumber.size() <
            static_cast<size_t>(MIN_ACCOUNT_NUMBER_LENGTH) ||
        accountNumber.size() >
            static_cast<size_t>(MAX_ACCOUNT_NUMBER_LENGTH)) {
        return false;
    }
    return isAllDigits(accountNumber);
}

bool isValidPin(const string& pin)
{
    return pin.size() == 4 && isAllDigits(pin);
}

bool isValidHolderName(const string& name)
{
    string cleaned = trim(name);
    if (cleaned.size() < 2 || cleaned.size() > 50) {
        return false;
    }
    if (!isalpha(static_cast<unsigned char>(cleaned.front())) ||
        !isalpha(static_cast<unsigned char>(cleaned.back()))) {
        return false;
    }

    int letterCount = 0;
    for (size_t i = 0; i < cleaned.size(); ++i) {
        unsigned char character = static_cast<unsigned char>(cleaned[i]);
        if (isalpha(character)) {
            ++letterCount;
        } else if (cleaned[i] != ' ' && cleaned[i] != '.' &&
                   cleaned[i] != '\'' && cleaned[i] != '-') {
            return false;
        }
    }
    return letterCount >= 2;
}

// ============================================================
//  CSV HELPERS
// ============================================================

string csvEscape(const string& field)
{
    if (field.find_first_of(",\"\r\n") == string::npos) {
        return field;
    }

    string escaped = "\"";
    for (size_t i = 0; i < field.size(); ++i) {
        if (field[i] == '"') {
            escaped += "\"\"";
        } else {
            escaped += field[i];
        }
    }
    escaped += '"';
    return escaped;
}

// A small CSV parser that supports quoted fields and doubled quotes.
vector<string> splitCsvLine(const string& line)
{
    vector<string> fields;
    string field;
    bool insideQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char current = line[i];

        if (insideQuotes) {
            if (current == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    insideQuotes = false;
                }
            } else {
                field += current;
            }
        } else if (current == '"') {
            insideQuotes = true;
        } else if (current == ',') {
            fields.push_back(field);
            field.clear();
        } else {
            field += current;
        }
    }
    fields.push_back(field);
    return fields;
}

// ============================================================
//  FILE HELPERS
// ============================================================

bool fileExists(const string& path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    ifstream file(path.c_str());
    return file.good();
#endif
}

bool replaceFile(const string& temporaryPath, const string& targetPath,
                 string& error)
{
#ifdef _WIN32
    if (MoveFileExA(temporaryPath.c_str(), targetPath.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
        return true;
    }
    ostringstream message;
    message << "Could not replace " << targetPath
            << " (Windows error " << GetLastError() << ").";
    error = message.str();
    return false;
#else
    if (rename(temporaryPath.c_str(), targetPath.c_str()) == 0) {
        return true;
    }
    error = "Could not replace " + targetPath + ".";
    return false;
#endif
}

// Write to a temporary file first, then replace the real file. This makes
// an interrupted write much less likely to destroy the previous data.
bool writeTextFileSafely(const string& path, const string& contents,
                         string& error)
{
    const string temporaryPath = path + ".tmp";
    ofstream output(temporaryPath.c_str(),
                    ios::out | ios::trunc | ios::binary);
    if (!output.is_open()) {
        error = "Could not open " + temporaryPath + " for writing.";
        return false;
    }

    output << contents;
    output.flush();
    if (!output.good()) {
        output.close();
        remove(temporaryPath.c_str());
        error = "Could not write all data to " + temporaryPath + ".";
        return false;
    }
    output.close();

    if (!replaceFile(temporaryPath, path, error)) {
        remove(temporaryPath.c_str());
        return false;
    }
    return true;
}

string backupFileNameFor(const string& originalPath)
{
    string stamp = currentDateTime();
    for (size_t i = 0; i < stamp.size(); ++i) {
        if (!isalnum(static_cast<unsigned char>(stamp[i]))) {
            stamp[i] = '-';
        }
    }

    string candidate = originalPath + ".backup-" + stamp;
    int suffix = 1;
    while (fileExists(candidate)) {
        candidate = originalPath + ".backup-" + stamp + "-" +
                    to_string(++suffix);
    }
    return candidate;
}

// Keeps a raw copy before a file containing skipped/corrupt rows is
// replaced by the cleaned in-memory data.
bool backupFile(const string& sourcePath, string& backupPath, string& error)
{
    if (!fileExists(sourcePath)) {
        error = "The original file does not exist.";
        return false;
    }

    backupPath = backupFileNameFor(sourcePath);

    ifstream source(sourcePath.c_str(), ios::in | ios::binary);
    if (!source.is_open()) {
        error = "Could not open " + sourcePath + " for backup.";
        return false;
    }

    ofstream destination(backupPath.c_str(),
                         ios::out | ios::trunc | ios::binary);
    if (!destination.is_open()) {
        error = "Could not create backup file " + backupPath + ".";
        return false;
    }

    destination << source.rdbuf();
    bool copiedSuccessfully = source.good() || source.eof();
    destination.close();
    if (!copiedSuccessfully || !destination.good()) {
        remove(backupPath.c_str());
        error = "Could not copy the complete file to " + backupPath + ".";
        return false;
    }
    return true;
}

bool ensureDataFile(const string& path, const string& header, string& error)
{
    if (!fileExists(path)) {
        ofstream output(path.c_str(), ios::out | ios::trunc);
        if (!output.is_open()) {
            error = "Could not create " + path + ".";
            return false;
        }
        output << header << "\n";
        if (!output.good()) {
            error = "Could not write the header to " + path + ".";
            return false;
        }
        return true;
    }

    ifstream input(path.c_str());
    if (!input.is_open()) {
        error = "The file " + path + " exists but could not be opened.";
        return false;
    }

    input.seekg(0, ios::end);
    if (input.tellg() == 0) {
        input.close();
        ofstream output(path.c_str(), ios::out | ios::trunc);
        if (!output.is_open()) {
            error = "Could not write the header to " + path + ".";
            return false;
        }
        output << header << "\n";
        if (!output.good()) {
            error = "Could not write the header to " + path + ".";
            return false;
        }
    }
    return true;
}

// ============================================================
//  TRANSACTION RECORD
// ============================================================

struct Transaction {
    string dateTime;
    string type;
    long long amountPaise;
    long long balancePaise;
    string reference;
    string note;

    Transaction()
        : amountPaise(0), balancePaise(0)
    {
    }

    Transaction(const string& transactionDateTime, const string& transactionType,
                long long transactionAmountPaise,
                long long balanceAfterPaise, const string& transactionReference,
                const string& transactionNote)
        : dateTime(transactionDateTime),
          type(transactionType),
          amountPaise(transactionAmountPaise),
          balancePaise(balanceAfterPaise),
          reference(transactionReference),
          note(transactionNote)
    {
    }
};

string transactionDisplayType(const string& type)
{
    if (type == "OPENING") {
        return "Opening";
    }
    if (type == "DEPOSIT") {
        return "Deposit";
    }
    if (type == "WITHDRAWAL") {
        return "Withdrawal";
    }
    if (type == "TRANSFER_OUT") {
        return "Transfer Out";
    }
    if (type == "TRANSFER_IN") {
        return "Transfer In";
    }
    return type;
}

bool transactionComesBefore(const Transaction& left, const Transaction& right)
{
    return left.dateTime < right.dateTime;
}

// ============================================================
//  CLASS: Account
//  Stores one account. Its data is private; outside code cannot
//  directly change the balance or PIN hash.
// ============================================================

class Account {
public:
    Account(const string& accountNumber, const string& holderName,
            const string& plainPin, long long openingBalancePaise,
            const string& createdAt);

    const string& getAccountNumber() const;
    const string& getHolderName() const;
    long long getBalance() const;

    bool verifyPIN(const string& pin) const;
    bool changePIN(const string& currentPin, const string& newPin,
                   const string& confirmPin, string& error);

    bool deposit(long long amountPaise, const string& reference,
                 const string& note, string& error, long long& newBalance);
    bool withdraw(long long amountPaise, long long dailyLimitPaise,
                  const string& reference, const string& note,
                  string& error, long long& newBalance);

    void displayMiniStatement() const;

private:
    // Bank is allowed to load saved records directly. This one friend
    // declaration keeps the data private from the rest of the program.
    friend class Bank;

    Account();  // Used only by Bank while loading data from disk

    string accountNumber_;
    string holderName_;
    string pinSalt_;
    string pinHash_;
    long long balancePaise_;
    string createdAt_;
    vector<Transaction> transactions_;

    bool credit(long long amountPaise, const string& type,
                const string& reference, const string& note,
                string& error, long long& newBalance);
    bool debit(long long amountPaise, long long dailyLimitPaise,
               const string& type, const string& reference, const string& note,
               string& error, long long& newBalance);

    void addTransaction(const string& type, long long amountPaise,
                        const string& reference, const string& note);
    long long totalWithdrawnToday() const;
};

Account::Account()
    : balancePaise_(0)
{
}

Account::Account(const string& accountNumber, const string& holderName,
                 const string& plainPin, long long openingBalancePaise,
                 const string& createdAt)
    : accountNumber_(accountNumber),
      holderName_(holderName),
      balancePaise_(openingBalancePaise),
      createdAt_(createdAt)
{
    pinSalt_ = generateSaltValue();
    pinHash_ = calculatePinHash(pinSalt_, plainPin);
}

const string& Account::getAccountNumber() const
{
    return accountNumber_;
}

const string& Account::getHolderName() const
{
    return holderName_;
}

long long Account::getBalance() const
{
    return balancePaise_;
}

bool Account::verifyPIN(const string& pin) const
{
    if (!isValidPin(pin)) {
        return false;
    }
    return calculatePinHash(pinSalt_, pin) == pinHash_;
}

bool Account::changePIN(const string& currentPin, const string& newPin,
                        const string& confirmPin, string& error)
{
    if (!verifyPIN(currentPin)) {
        error = "The current PIN is incorrect.";
        return false;
    }
    if (!isValidPin(newPin)) {
        error = "The new PIN must be exactly four digits.";
        return false;
    }
    if (newPin != confirmPin) {
        error = "The new PIN and its confirmation do not match.";
        return false;
    }
    if (newPin == currentPin) {
        error = "The new PIN must be different from the current PIN.";
        return false;
    }

    pinSalt_ = generateSaltValue();
    pinHash_ = calculatePinHash(pinSalt_, newPin);
    return true;
}

void Account::addTransaction(const string& type, long long amountPaise,
                             const string& reference, const string& note)
{
    transactions_.push_back(Transaction(currentDateTime(), type, amountPaise,
                                        balancePaise_, reference, note));
}

bool Account::credit(long long amountPaise, const string& type,
                     const string& reference, const string& note,
                     string& error, long long& newBalance)
{
    if (amountPaise <= 0) {
        error = "The amount must be greater than zero.";
        return false;
    }
    if (balancePaise_ > LLONG_MAX - amountPaise) {
        error = "The amount is too large to process.";
        return false;
    }

    balancePaise_ += amountPaise;
    addTransaction(type, amountPaise, reference, note);
    newBalance = balancePaise_;
    return true;
}

bool Account::debit(long long amountPaise, long long dailyLimitPaise,
                    const string& type, const string& reference,
                    const string& note, string& error, long long& newBalance)
{
    if (amountPaise <= 0) {
        error = "The amount must be greater than zero.";
        return false;
    }
    if (amountPaise > balancePaise_) {
        error = "Insufficient balance. Available balance: " +
                formatMoney(balancePaise_) + ".";
        return false;
    }

    if (type == "WITHDRAWAL" && dailyLimitPaise > 0) {
        long long alreadyWithdrawn = totalWithdrawnToday();
        long long remaining =
            (alreadyWithdrawn >= dailyLimitPaise)
                ? 0
                : dailyLimitPaise - alreadyWithdrawn;

        if (amountPaise > remaining) {
            error = "Daily withdrawal limit exceeded. Remaining limit today: " +
                    formatMoney(remaining) + ".";
            return false;
        }
    }

    balancePaise_ -= amountPaise;
    addTransaction(type, amountPaise, reference, note);
    newBalance = balancePaise_;
    return true;
}

long long Account::totalWithdrawnToday() const
{
    const string today = todayDate();
    long long total = 0;

    for (size_t i = 0; i < transactions_.size(); ++i) {
        const Transaction& transaction = transactions_[i];
        if (transaction.type == "WITHDRAWAL" &&
            transaction.dateTime.substr(0, 10) == today) {
            if (total > LLONG_MAX - transaction.amountPaise) {
                return LLONG_MAX;
            }
            total += transaction.amountPaise;
        }
    }
    return total;
}

bool Account::deposit(long long amountPaise, const string& reference,
                      const string& note, string& error,
                      long long& newBalance)
{
    return credit(amountPaise, "DEPOSIT", reference, note, error, newBalance);
}

bool Account::withdraw(long long amountPaise, long long dailyLimitPaise,
                       const string& reference, const string& note,
                       string& error, long long& newBalance)
{
    return debit(amountPaise, dailyLimitPaise, "WITHDRAWAL", reference, note,
                 error, newBalance);
}

void Account::displayMiniStatement() const
{
    cout << "\n=================== MINI STATEMENT ===================\n";
    cout << "Account Holder : " << holderName_ << "\n";
    cout << "Account Number : " << accountNumber_ << "\n";

    if (transactions_.empty()) {
        cout << "\nNo transactions found for this account.\n";
        cout << "======================================================\n";
        return;
    }

    size_t numberOfRows =
        (transactions_.size() < 5) ? transactions_.size() : 5;
    cout << "Showing the last " << numberOfRows
         << " transaction(s), oldest first.\n\n";

    cout << left << setw(20) << "Date & Time"
         << setw(16) << "Type"
         << right << setw(19) << "Amount"
         << setw(19) << "Balance"
         << "  Reference\n";
    cout << string(104, '-') << "\n";

    size_t start = (transactions_.size() > 5) ? transactions_.size() - 5 : 0;
    for (size_t i = start; i < transactions_.size(); ++i) {
        const Transaction& transaction = transactions_[i];
        bool isCredit = (transaction.type == "DEPOSIT" ||
                         transaction.type == "TRANSFER_IN" ||
                         transaction.type == "OPENING");
        string signedAmount = (isCredit ? "+" : "-") +
                              formatMoney(transaction.amountPaise);

        cout << left << setw(20) << transaction.dateTime
             << setw(16) << transactionDisplayType(transaction.type)
             << right << setw(19) << signedAmount
             << setw(19) << formatMoney(transaction.balancePaise)
             << "  " << transaction.reference << "\n";
    }

    cout << string(104, '-') << "\n";
    cout << "Current Balance: " << formatMoney(balancePaise_) << "\n";
    cout << "======================================================\n";
}

// ============================================================
//  CLASS: Bank
//  Owns every Account, loads/saves both CSV files, authenticates
//  users, and coordinates deposits, withdrawals and transfers.
// ============================================================

class Bank {
public:
    Bank(const string& accountsFile = ACCOUNTS_FILE,
         const string& transactionsFile = TRANSACTIONS_FILE);

    bool load(string& error);
    bool saveAll(string& error) const;

    bool createAccount(const string& requestedAccountNumber,
                       const string& holderName, const string& pin,
                       long long initialDepositPaise,
                       string& createdAccountNumber, string& error);

    Account* findAccount(const string& accountNumber);
    const Account* findAccount(const string& accountNumber) const;
    Account* authenticate(const string& accountNumber, const string& pin);

    bool deposit(const string& accountNumber, long long amountPaise,
                 string& reference, string& error);
    bool withdraw(const string& accountNumber, long long amountPaise,
                  string& reference, string& error);
    bool changePIN(const string& accountNumber, const string& currentPin,
                   const string& newPin, const string& confirmPin,
                   string& error);
    bool transfer(const string& fromAccountNumber,
                  const string& toAccountNumber, long long amountPaise,
                  string& reference, string& recipientName, string& error);

private:
    vector<Account> accounts_;
    string accountsFile_;
    string transactionsFile_;

    bool saveAccounts(string& error) const;
    bool saveTransactions(string& error) const;
    string generateUniqueAccountNumber();
    string generateUniqueReference() const;
    bool referenceExists(const string& reference) const;
};

Bank::Bank(const string& accountsFile, const string& transactionsFile)
    : accountsFile_(accountsFile), transactionsFile_(transactionsFile)
{
}

Account* Bank::findAccount(const string& accountNumber)
{
    for (size_t i = 0; i < accounts_.size(); ++i) {
        if (accounts_[i].accountNumber_ == accountNumber) {
            return &accounts_[i];
        }
    }
    return NULL;
}

const Account* Bank::findAccount(const string& accountNumber) const
{
    for (size_t i = 0; i < accounts_.size(); ++i) {
        if (accounts_[i].accountNumber_ == accountNumber) {
            return &accounts_[i];
        }
    }
    return NULL;
}

Account* Bank::authenticate(const string& accountNumber, const string& pin)
{
    Account* account = findAccount(accountNumber);
    if (account == NULL) {
        return NULL;
    }
    if (!account->verifyPIN(pin)) {
        return NULL;
    }
    return account;
}

string Bank::generateUniqueAccountNumber()
{
    while (true) {
        string candidate = generateTenDigitAccountNumber();
        if (findAccount(candidate) == NULL) {
            return candidate;
        }
    }
}

bool Bank::referenceExists(const string& reference) const
{
    for (size_t i = 0; i < accounts_.size(); ++i) {
        const vector<Transaction>& transactions = accounts_[i].transactions_;
        for (size_t j = 0; j < transactions.size(); ++j) {
            if (!reference.empty() && transactions[j].reference == reference) {
                return true;
            }
        }
    }
    return false;
}

string Bank::generateUniqueReference() const
{
    // The free helper's counter restarts with each program run. Checking
    // loaded history prevents duplicate references for transactions made
    // within the same second of restarting the application.
    for (int attempt = 0; attempt < 1000; ++attempt) {
        string candidate = generateTransactionReference();
        if (!referenceExists(candidate)) {
            return candidate;
        }
    }

    // Extremely unlikely fallback that includes a random value.
    return "TXN-" + randomHexString(16);
}

bool Bank::load(string& error)
{
    accounts_.clear();

    if (!ensureDataFile(accountsFile_, ACCOUNTS_HEADER, error)) {
        return false;
    }
    if (!ensureDataFile(transactionsFile_, TRANSACTIONS_HEADER, error)) {
        return false;
    }

    ifstream accountInput(accountsFile_.c_str());
    if (!accountInput.is_open()) {
        error = "Could not open " + accountsFile_ + " for reading.";
        return false;
    }

    int accountLineNumber = 0;
    int malformedAccounts = 0;
    int duplicateAccounts = 0;
    string line;

    while (getline(accountInput, line)) {
        ++accountLineNumber;
        if (trim(line).empty()) {
            continue;
        }

        vector<string> fields = splitCsvLine(line);
        if (accountLineNumber == 1 && !fields.empty() &&
            trim(fields[0]) == "account_no") {
            continue;  // CSV header
        }

        if (fields.size() != 6) {
            ++malformedAccounts;
            continue;
        }

        string number = trim(fields[0]);
        string name = trim(fields[1]);
        string salt = trim(fields[2]);
        string hash = trim(fields[3]);
        string balanceText = trim(fields[4]);
        string createdAt = trim(fields[5]);
        long long balance = 0;

        bool validRecord =
            isValidAccountNumber(number) && isValidHolderName(name) &&
            salt.size() == 32 && hash.size() == 16 &&
            isHexString(salt) && isHexString(hash) &&
            parseNonNegativeInteger(balanceText, balance);

        if (!validRecord) {
            ++malformedAccounts;
            continue;
        }
        if (findAccount(number) != NULL) {
            ++duplicateAccounts;
            continue;
        }

        Account loadedAccount;
        loadedAccount.accountNumber_ = number;
        loadedAccount.holderName_ = name;
        loadedAccount.pinSalt_ = salt;
        loadedAccount.pinHash_ = hash;
        loadedAccount.balancePaise_ = balance;
        loadedAccount.createdAt_ = createdAt.empty()
                                       ? currentDateTime()
                                       : createdAt;
        accounts_.push_back(loadedAccount);
    }
    accountInput.close();

    ifstream transactionInput(transactionsFile_.c_str());
    if (!transactionInput.is_open()) {
        error = "Could not open " + transactionsFile_ + " for reading.";
        return false;
    }

    int transactionLineNumber = 0;
    int malformedTransactions = 0;
    int orphanTransactions = 0;
    int loadedTransactions = 0;

    while (getline(transactionInput, line)) {
        ++transactionLineNumber;
        if (trim(line).empty()) {
            continue;
        }

        vector<string> fields = splitCsvLine(line);
        if (transactionLineNumber == 1 && !fields.empty() &&
            trim(fields[0]) == "account_no") {
            continue;  // CSV header
        }

        if (fields.size() != 7) {
            ++malformedTransactions;
            continue;
        }

        string number = trim(fields[0]);
        string dateTime = trim(fields[1]);
        string type = trim(fields[2]);
        string amountText = trim(fields[3]);
        string balanceText = trim(fields[4]);
        string reference = trim(fields[5]);
        string note = fields[6];

        long long amount = 0;
        long long balanceAfter = 0;

        Account* owner = findAccount(number);
        if (owner == NULL) {
            ++orphanTransactions;
            continue;
        }

        bool validRecord =
            !dateTime.empty() && !type.empty() &&
            parseNonNegativeInteger(amountText, amount) &&
            parseNonNegativeInteger(balanceText, balanceAfter) &&
            amount > 0;

        if (!validRecord) {
            ++malformedTransactions;
            continue;
        }

        owner->transactions_.push_back(
            Transaction(dateTime, type, amount, balanceAfter, reference, note));
        ++loadedTransactions;
    }
    transactionInput.close();

    // Keep every account's history in chronological order. The sort is
    // stable so records created in the same second keep their file order.
    for (size_t i = 0; i < accounts_.size(); ++i) {
        stable_sort(accounts_[i].transactions_.begin(),
                    accounts_[i].transactions_.end(),
                    transactionComesBefore);
    }

    cout << "Loaded " << accounts_.size() << " account(s) and "
         << loadedTransactions << " transaction(s) from local files.\n";

    if (malformedAccounts > 0) {
        cout << "Warning: skipped " << malformedAccounts
             << " malformed account record(s).\n";
    }
    if (duplicateAccounts > 0) {
        cout << "Warning: skipped " << duplicateAccounts
             << " duplicate account number(s).\n";
    }
    if (malformedTransactions > 0) {
        cout << "Warning: skipped " << malformedTransactions
             << " malformed transaction record(s).\n";
    }
    if (orphanTransactions > 0) {
        cout << "Warning: skipped " << orphanTransactions
             << " transaction(s) with unknown account numbers.\n";
    }

    // A skipped row is kept in a raw backup before the next save rewrites
    // the CSV without it. This prevents silent loss of corrupted records.
    if (malformedAccounts > 0 || duplicateAccounts > 0) {
        string backupPath;
        string backupError;
        if (backupFile(accountsFile_, backupPath, backupError)) {
            cout << "A raw safety copy of " << accountsFile_
                 << " was created at " << backupPath << ".\n";
        } else {
            cout << "Warning: could not back up " << accountsFile_ << ". "
                 << backupError << "\n";
        }
    }
    if (malformedTransactions > 0 || orphanTransactions > 0) {
        string backupPath;
        string backupError;
        if (backupFile(transactionsFile_, backupPath, backupError)) {
            cout << "A raw safety copy of " << transactionsFile_
                 << " was created at " << backupPath << ".\n";
        } else {
            cout << "Warning: could not back up " << transactionsFile_
                 << ". " << backupError << "\n";
        }
    }

    return true;
}

bool Bank::saveAccounts(string& error) const
{
    ostringstream contents;
    contents << ACCOUNTS_HEADER << "\n";

    for (size_t i = 0; i < accounts_.size(); ++i) {
        const Account& account = accounts_[i];
        contents << csvEscape(account.accountNumber_) << ','
                 << csvEscape(account.holderName_) << ',' << account.pinSalt_
                 << ',' << account.pinHash_ << ',' << account.balancePaise_
                 << ',' << csvEscape(account.createdAt_) << "\n";
    }

    return writeTextFileSafely(accountsFile_, contents.str(), error);
}

bool Bank::saveTransactions(string& error) const
{
    ostringstream contents;
    contents << TRANSACTIONS_HEADER << "\n";

    for (size_t i = 0; i < accounts_.size(); ++i) {
        const Account& account = accounts_[i];
        for (size_t j = 0; j < account.transactions_.size(); ++j) {
            const Transaction& transaction = account.transactions_[j];
            contents << csvEscape(account.accountNumber_) << ','
                     << csvEscape(transaction.dateTime) << ','
                     << csvEscape(transaction.type) << ','
                     << transaction.amountPaise << ','
                     << transaction.balancePaise << ','
                     << csvEscape(transaction.reference) << ','
                     << csvEscape(transaction.note) << "\n";
        }
    }

    return writeTextFileSafely(transactionsFile_, contents.str(), error);
}

bool Bank::saveAll(string& error) const
{
    if (!saveAccounts(error)) {
        return false;
    }
    if (!saveTransactions(error)) {
        return false;
    }
    return true;
}

bool Bank::createAccount(const string& requestedAccountNumber,
                         const string& holderName, const string& pin,
                         long long initialDepositPaise,
                         string& createdAccountNumber, string& error)
{
    string name = trim(holderName);
    if (!isValidHolderName(name)) {
        error = "Enter a valid full name (2 to 50 letters, spaces, dots, "
                "apostrophes or hyphens).";
        return false;
    }
    if (!isValidPin(pin)) {
        error = "The PIN must be exactly four digits.";
        return false;
    }
    if (initialDepositPaise < 0) {
        error = "The initial deposit cannot be negative.";
        return false;
    }

    string number = trim(requestedAccountNumber);
    if (number.empty()) {
        number = generateUniqueAccountNumber();
    } else if (!isValidAccountNumber(number)) {
        error = "The account number must contain 6 to 12 digits only.";
        return false;
    }

    if (findAccount(number) != NULL) {
        error = "That account number already exists. Choose another one.";
        return false;
    }

    Account newAccount(number, name, pin, initialDepositPaise,
                       currentDateTime());

    // The constructor already set the opening balance. Record the matching
    // OPENING transaction without crediting the money a second time. No row
    // is needed for a zero-rupee opening balance.
    if (initialDepositPaise > 0) {
        newAccount.addTransaction("OPENING", initialDepositPaise,
                                  generateUniqueReference(),
                                  "Opening deposit");
    }

    accounts_.push_back(newAccount);

    if (!saveAll(error)) {
        string saveError = error;
        accounts_.pop_back();
        string rollbackError;
        saveAll(rollbackError);
        error = "Account creation was cancelled because data could not be "
                "saved. " + saveError;
        return false;
    }

    createdAccountNumber = number;
    return true;
}

bool Bank::deposit(const string& accountNumber, long long amountPaise,
                   string& reference, string& error)
{
    Account* account = findAccount(accountNumber);
    if (account == NULL) {
        error = "The account could not be found.";
        return false;
    }

    Account backup = *account;
    string newReference = generateUniqueReference();
    long long newBalance = 0;

    if (!account->deposit(amountPaise, newReference,
                          "Cash deposit at PyBank ATM", error, newBalance)) {
        return false;
    }

    if (!saveAll(error)) {
        string saveError = error;
        *account = backup;
        string rollbackError;
        saveAll(rollbackError);
        error = "Deposit cancelled because data could not be saved. " +
                saveError;
        return false;
    }

    reference = newReference;
    return true;
}

bool Bank::withdraw(const string& accountNumber, long long amountPaise,
                    string& reference, string& error)
{
    Account* account = findAccount(accountNumber);
    if (account == NULL) {
        error = "The account could not be found.";
        return false;
    }

    Account backup = *account;
    string newReference = generateUniqueReference();
    long long newBalance = 0;

    if (!account->withdraw(amountPaise, DAILY_WITHDRAWAL_LIMIT_PAISE,
                           newReference, "Cash withdrawal at PyBank ATM",
                           error, newBalance)) {
        return false;
    }

    if (!saveAll(error)) {
        string saveError = error;
        *account = backup;
        string rollbackError;
        saveAll(rollbackError);
        error = "Withdrawal cancelled because data could not be saved. " +
                saveError;
        return false;
    }

    reference = newReference;
    return true;
}

bool Bank::changePIN(const string& accountNumber, const string& currentPin,
                     const string& newPin, const string& confirmPin,
                     string& error)
{
    Account* account = findAccount(accountNumber);
    if (account == NULL) {
        error = "The account could not be found.";
        return false;
    }

    Account backup = *account;

    if (!account->changePIN(currentPin, newPin, confirmPin, error)) {
        return false;
    }

    if (!saveAccounts(error)) {
        string saveError = error;
        *account = backup;
        string rollbackError;
        saveAccounts(rollbackError);
        error = "PIN change cancelled because data could not be saved. " +
                saveError;
        return false;
    }

    return true;
}

bool Bank::transfer(const string& fromAccountNumber,
                    const string& toAccountNumber, long long amountPaise,
                    string& reference, string& recipientName, string& error)
{
    if (fromAccountNumber == toAccountNumber) {
        error = "You cannot transfer money to the same account.";
        return false;
    }
    if (amountPaise <= 0) {
        error = "The transfer amount must be greater than zero.";
        return false;
    }

    Account* sender = findAccount(fromAccountNumber);
    if (sender == NULL) {
        error = "The sender account could not be found.";
        return false;
    }

    Account* receiver = findAccount(toAccountNumber);
    if (receiver == NULL) {
        error = "The recipient account does not exist.";
        return false;
    }

    if (amountPaise > sender->balancePaise_) {
        error = "Insufficient balance. Available balance: " +
                formatMoney(sender->balancePaise_) + ".";
        return false;
    }

    Account senderBackup = *sender;
    Account receiverBackup = *receiver;

    string newReference = generateUniqueReference();
    string outgoingNote = "Transfer to " + receiver->holderName_ + " (A/C " +
                          receiver->accountNumber_ + ")";
    string incomingNote = "Transfer from " + sender->holderName_ + " (A/C " +
                          sender->accountNumber_ + ")";

    long long ignoredBalance = 0;

    if (!sender->debit(amountPaise, 0, "TRANSFER_OUT", newReference,
                       outgoingNote, error, ignoredBalance)) {
        return false;
    }

    if (!receiver->credit(amountPaise, "TRANSFER_IN", newReference,
                          incomingNote, error, ignoredBalance)) {
        *sender = senderBackup;
        return false;
    }

    if (!saveAll(error)) {
        string saveError = error;
        *sender = senderBackup;
        *receiver = receiverBackup;
        string rollbackError;
        saveAll(rollbackError);
        error = "Transfer cancelled because data could not be saved. " +
                saveError;
        return false;
    }

    reference = newReference;
    recipientName = receiver->holderName_;
    return true;
}

// ============================================================
//  MENU INPUT HELPER
// ============================================================

bool parseMenuNumber(const string& text, int& value)
{
    long long parsed = 0;
    if (!parseNonNegativeInteger(text, parsed)) {
        return false;
    }
    if (parsed > 1000000) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

int readIntInRange(const string& prompt, int minimum, int maximum)
{
    while (true) {
        string text = readLine(prompt);
        int value = 0;
        if (parseMenuNumber(text, value) && value >= minimum &&
            value <= maximum) {
            return value;
        }
        cout << "Invalid input. Please enter a number from " << minimum
             << " to " << maximum << ".\n";
    }
}

// ============================================================
//  CLASS: ATM
//  The terminal user interface. It owns one Bank object and only
//  displays menus/reads input; Bank performs all banking changes.
// ============================================================

class ATM {
public:
    ATM();
    bool initialize();
    void run();

private:
    Bank bank_;
    int consecutiveLoginFailures_;
    bool running_;

    void showWelcomeMenu() const;
    void createAccountFlow();
    void loginFlow();
    void runAccountSession(Account& account);
    void showAccountMenu(const Account& account) const;

    void checkBalance(const Account& account);
    void depositMoney(Account& account);
    void withdrawMoney(Account& account);
    void showMiniStatement(Account& account);
    void changePINFlow(Account& account);
    void transferMoney(Account& account);

    void printReceipt(const string& transactionType, const Account& account,
                      long long amountPaise, const string& reference,
                      const string& details) const;
};

ATM::ATM() : consecutiveLoginFailures_(0), running_(true)
{
}

bool ATM::initialize()
{
    string error;
    if (!bank_.load(error)) {
        cout << "\nStartup error: " << error << "\n";
        return false;
    }
    return true;
}

void ATM::showWelcomeMenu() const
{
    cout << "\n=================================================\n";
    cout << "               PYBANK ATM SIMULATOR\n";
    cout << "          Educational project - not a bank\n";
    cout << "=================================================\n";
    cout << "1. Create New Account\n";
    cout << "2. Login\n";
    cout << "3. Exit\n";
    cout << "-------------------------------------------------\n";
}

void ATM::createAccountFlow()
{
    cout << "\n---------------- CREATE NEW ACCOUNT ----------------\n";

    string holderName = readLine("Enter account holder's full name: ");
    if (!isValidHolderName(holderName)) {
        cout << "\nError: Enter a valid name (2 to 50 letters; spaces, dots, "
                "apostrophes and hyphens are allowed).\n";
        pauseScreen();
        return;
    }

    string requestedNumber = readLine(
        "Enter account number (6-12 digits, or press Enter to generate): ");
    if (!requestedNumber.empty() &&
        !isValidAccountNumber(requestedNumber)) {
        cout << "\nError: The account number must contain 6 to 12 digits "
                "only.\n";
        pauseScreen();
        return;
    }
    if (!requestedNumber.empty() &&
        bank_.findAccount(requestedNumber) != NULL) {
        cout << "\nError: That account number already exists. Please choose "
                "a different number.\n";
        pauseScreen();
        return;
    }

    string pin = readPassword("Set a 4-digit PIN: ");
    if (!isValidPin(pin)) {
        cout << "\nError: The PIN must be exactly four digits.\n";
        pauseScreen();
        return;
    }

    string confirmPin = readPassword("Confirm the PIN: ");
    if (pin != confirmPin) {
        cout << "\nError: The two PIN entries do not match.\n";
        pauseScreen();
        return;
    }

    string initialDepositText = readLine(
        "Enter initial deposit in Rs. (0 for none): ");
    long long initialDepositPaise = 0;
    if (!initialDepositText.empty() &&
        !parseRupeesToPaise(initialDepositText, initialDepositPaise)) {
        cout << "\nError: Enter a valid non-negative amount with at most two "
                "decimal places (for example, 500 or 500.50).\n";
        pauseScreen();
        return;
    }

    string accountNumber;
    string error;
    if (!bank_.createAccount(requestedNumber, holderName, pin,
                             initialDepositPaise, accountNumber, error)) {
        cout << "\nError: " << error << "\n";
        pauseScreen();
        return;
    }

    cout << "\nAccount created successfully!\n";
    cout << "Account Number  : " << accountNumber << "\n";
    cout << "Account Holder  : " << trim(holderName) << "\n";
    cout << "Opening Balance : " << formatMoney(initialDepositPaise) << "\n";
    cout << "\nRemember your account number and PIN. The PIN is stored only "
            "as a salted hash, never as plain text.\n";
    pauseScreen();
}

void ATM::loginFlow()
{
    if (consecutiveLoginFailures_ >= MAX_LOGIN_ATTEMPTS) {
        cout << "\nLogin is locked for this application session because of "
                "too many failed attempts.\n";
        cout << "Please exit and restart PyBank ATM to reset the login "
                "lock.\n";
        pauseScreen();
        return;
    }

    cout << "\n----------------------- LOGIN -----------------------\n";

    while (consecutiveLoginFailures_ < MAX_LOGIN_ATTEMPTS) {
        string accountNumber = readLine("Enter account number: ");
        string pin = readPassword("Enter PIN: ");

        Account* account = NULL;
        if (isValidAccountNumber(accountNumber)) {
            account = bank_.authenticate(accountNumber, pin);
        }

        if (account != NULL) {
            consecutiveLoginFailures_ = 0;
            cout << "\nLogin successful. Welcome, "
                 << account->getHolderName() << "!\n";
            runAccountSession(*account);
            return;
        }

        ++consecutiveLoginFailures_;
        int remaining = MAX_LOGIN_ATTEMPTS - consecutiveLoginFailures_;

        cout << "\nInvalid account number or PIN.";
        if (remaining > 0) {
            cout << " Attempts remaining: " << remaining << ".\n";
        } else {
            cout << "\nToo many consecutive failed attempts. Login is locked "
                    "for this session.\n";
        }
    }

    pauseScreen();
}

void ATM::showAccountMenu(const Account& account) const
{
    cout << "\n=================================================\n";
    cout << "               WELCOME TO PYBANK ATM\n";
    cout << "=================================================\n";
    cout << "Logged in as : " << account.getHolderName() << "\n";
    cout << "Account No.  : " << account.getAccountNumber() << "\n";
    cout << "-------------------------------------------------\n";
    cout << "1. Check Balance\n";
    cout << "2. Deposit Money\n";
    cout << "3. Withdraw Money\n";
    cout << "4. Mini Statement\n";
    cout << "5. Change PIN\n";
    cout << "6. Transfer Money\n";
    cout << "7. Logout\n";
    cout << "8. Exit\n";
    cout << "-------------------------------------------------\n";
}

void ATM::runAccountSession(Account& account)
{
    bool loggedIn = true;

    while (loggedIn && running_) {
        showAccountMenu(account);
        int choice = readIntInRange("Enter your choice: ", 1, 8);

        switch (choice) {
        case 1:
            checkBalance(account);
            break;
        case 2:
            depositMoney(account);
            break;
        case 3:
            withdrawMoney(account);
            break;
        case 4:
            showMiniStatement(account);
            break;
        case 5:
            changePINFlow(account);
            break;
        case 6:
            transferMoney(account);
            break;
        case 7:
            cout << "\nLogging out safely. Your data has been saved.\n";
            loggedIn = false;
            break;
        case 8:
            cout << "\nExiting PyBank ATM. Your data has been saved.\n";
            loggedIn = false;
            running_ = false;
            break;
        default:
            // readIntInRange already prevents this branch.
            break;
        }
    }
}

void ATM::checkBalance(const Account& account)
{
    cout << "\n------------------- ACCOUNT BALANCE -------------------\n";
    cout << "Account Holder    : " << account.getHolderName() << "\n";
    cout << "Account Number    : " << account.getAccountNumber() << "\n";
    cout << "Available Balance : " << formatMoney(account.getBalance())
         << "\n";
    cout << "-------------------------------------------------------\n";
    pauseScreen();
}

void ATM::depositMoney(Account& account)
{
    cout << "\n------------------- DEPOSIT MONEY ---------------------\n";
    string amountText =
        readLine("Enter deposit amount in Rs. (Enter to cancel): ");
    if (amountText.empty()) {
        cout << "\nDeposit cancelled. No changes were made.\n";
        pauseScreen();
        return;
    }

    long long amountPaise = 0;
    if (!parseRupeesToPaise(amountText, amountPaise)) {
        cout << "\nError: Enter a valid amount with at most two decimal "
                "places.\n";
        pauseScreen();
        return;
    }
    if (amountPaise <= 0) {
        cout << "\nError: The deposit amount must be greater than zero.\n";
        pauseScreen();
        return;
    }

    string reference;
    string error;
    if (!bank_.deposit(account.getAccountNumber(), amountPaise, reference,
                       error)) {
        cout << "\nError: " << error << "\n";
        pauseScreen();
        return;
    }

    cout << "\nDeposit successful.\n";
    printReceipt("DEPOSIT", account, amountPaise, reference,
                 "Cash deposit at PyBank ATM");
    pauseScreen();
}

void ATM::withdrawMoney(Account& account)
{
    cout << "\n------------------ WITHDRAW MONEY --------------------\n";
    if (DAILY_WITHDRAWAL_LIMIT_PAISE > 0) {
        cout << "Daily cash withdrawal limit: "
             << formatMoney(DAILY_WITHDRAWAL_LIMIT_PAISE) << "\n";
    }

    string amountText =
        readLine("Enter withdrawal amount in Rs. (Enter to cancel): ");
    if (amountText.empty()) {
        cout << "\nWithdrawal cancelled. No changes were made.\n";
        pauseScreen();
        return;
    }

    long long amountPaise = 0;
    if (!parseRupeesToPaise(amountText, amountPaise)) {
        cout << "\nError: Enter a valid amount with at most two decimal "
                "places.\n";
        pauseScreen();
        return;
    }
    if (amountPaise <= 0) {
        cout << "\nError: The withdrawal amount must be greater than zero.\n";
        pauseScreen();
        return;
    }

    string reference;
    string error;
    if (!bank_.withdraw(account.getAccountNumber(), amountPaise, reference,
                        error)) {
        cout << "\nError: " << error << "\n";
        pauseScreen();
        return;
    }

    cout << "\nWithdrawal successful. Please collect your cash.\n";
    printReceipt("WITHDRAWAL", account, amountPaise, reference,
                 "Cash withdrawal at PyBank ATM");
    pauseScreen();
}

void ATM::showMiniStatement(Account& account)
{
    account.displayMiniStatement();
    pauseScreen();
}

void ATM::changePINFlow(Account& account)
{
    cout << "\n--------------------- CHANGE PIN ---------------------\n";
    cout << "For your security, verify your current PIN first.\n";

    string currentPin = readPassword("Enter current PIN: ");
    string newPin = readPassword("Enter new 4-digit PIN: ");
    string confirmPin = readPassword("Confirm new PIN: ");

    string error;
    if (!bank_.changePIN(account.getAccountNumber(), currentPin, newPin,
                         confirmPin, error)) {
        cout << "\nError: " << error << "\n";
        pauseScreen();
        return;
    }

    cout << "\nPIN changed successfully. Use the new PIN at the next "
            "login.\n";
    pauseScreen();
}

void ATM::transferMoney(Account& account)
{
    cout << "\n-------------------- TRANSFER MONEY ------------------\n";

    string recipientNumber = readLine("Enter recipient account number: ");
    if (!isValidAccountNumber(recipientNumber)) {
        cout << "\nError: The recipient account number must contain 6 to 12 "
                "digits only.\n";
        pauseScreen();
        return;
    }

    string amountText =
        readLine("Enter transfer amount in Rs. (Enter to cancel): ");
    if (amountText.empty()) {
        cout << "\nTransfer cancelled. No changes were made.\n";
        pauseScreen();
        return;
    }

    long long amountPaise = 0;
    if (!parseRupeesToPaise(amountText, amountPaise)) {
        cout << "\nError: Enter a valid amount with at most two decimal "
                "places.\n";
        pauseScreen();
        return;
    }
    if (amountPaise <= 0) {
        cout << "\nError: The transfer amount must be greater than zero.\n";
        pauseScreen();
        return;
    }

    string reference;
    string recipientName;
    string error;
    if (!bank_.transfer(account.getAccountNumber(), recipientNumber,
                        amountPaise, reference, recipientName, error)) {
        cout << "\nError: " << error << "\n";
        pauseScreen();
        return;
    }

    cout << "\nTransfer successful to " << recipientName << " (A/C "
         << recipientNumber << ").\n";
    printReceipt("TRANSFER_OUT", account, amountPaise, reference,
                 "To: " + recipientName + " (A/C " + recipientNumber + ")");
    pauseScreen();
}

void ATM::printReceipt(const string& transactionType, const Account& account,
                       long long amountPaise, const string& reference,
                       const string& details) const
{
    cout << "\n----------------- PYBANK RECEIPT -----------------\n";
    cout << "Reference   : " << reference << "\n";
    cout << "Date/Time   : " << currentDateTime() << "\n";
    cout << "Account No. : " << account.getAccountNumber() << "\n";
    cout << "Holder      : " << account.getHolderName() << "\n";
    cout << "Transaction : " << transactionDisplayType(transactionType)
         << "\n";
    cout << "Amount      : " << formatMoney(amountPaise) << "\n";
    if (!details.empty()) {
        cout << "Details     : " << details << "\n";
    }
    cout << "Balance     : " << formatMoney(account.getBalance()) << "\n";
    cout << "--------------------------------------------------\n";
    cout << "Educational simulator - not a real bank receipt.\n";
}

void ATM::run()
{
    while (running_) {
        showWelcomeMenu();
        int choice = readIntInRange("Enter your choice: ", 1, 3);

        switch (choice) {
        case 1:
            createAccountFlow();
            break;
        case 2:
            loginFlow();
            break;
        case 3:
            cout << "\nExiting PyBank ATM...\n";
            running_ = false;
            break;
        default:
            break;
        }
    }

    string error;
    if (!bank_.saveAll(error)) {
        cout << "\nWarning: the final save failed. " << error << "\n";
        cout << "Please check the data files before shutting down.\n";
    } else {
        cout << "\nAll data has been saved. Thank you for using PyBank ATM!\n";
    }
}

// ============================================================
//  PROGRAM ENTRY POINT
// ============================================================

int main()
{
    ATM atm;
    if (!atm.initialize()) {
        cout << "\nPyBank ATM could not start. Please check the data files "
                "and try again.\n";
        return 1;
    }

    atm.run();
    return 0;
}
