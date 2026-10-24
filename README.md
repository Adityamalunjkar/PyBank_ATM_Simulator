# PyBank ATM Simulator

A terminal-based **Simple Banking / ATM Simulator** written in C++11 for the
*Mini Project Ideas – OOPS*, Project No. 5.

> **Educational project only.** PyBank is not real banking software. Never use
> real bank account numbers, real PINs or real money with this program.

---

## 1. Objective

Build a menu-driven ATM simulator that demonstrates:

- classes, objects, constructors and encapsulation;
- conditionals, loops and reusable input validation;
- local file handling for persistent data;
- safe handling of money using integer paise instead of floating point;
- a simple salted PIN hash instead of storing PINs as plain text.

The program runs entirely in a terminal. It uses no GUI, website, external
database or third-party library.

---

## 2. Features

### Mandatory features (all implemented)

| # | Feature | What the program does |
|---|---------|-----------------------|
| A | Account creation | Full name, 6–12 digit account number (or auto-generate), 4-digit PIN and optional opening deposit (zero allowed). Duplicate numbers are rejected. |
| B | Secure login | Account number + PIN, maximum three consecutive failures, login locked for the rest of the run. PIN is masked in interactive Windows/Linux terminals. |
| C | ATM menu | Check Balance, Deposit, Withdraw, Mini Statement, Change PIN, Transfer, Logout, Exit. The session repeats until logout/exit. |
| D | Check balance | Shows the logged-in account's balance as `Rs. 1,23,456.78`. Other accounts cannot be accessed from a session. |
| E | Deposit | Rejects letters, empty/zero/negative values and overflow. Records type, amount, date/time, reference and new balance. |
| F | Withdraw | Rejects invalid and zero amounts, insufficient funds and amounts exceeding the configured daily limit. Records each successful withdrawal. |
| G | Mini statement | Last five transactions, oldest first within that window, with date/time, type, amount, resulting balance and reference. Shows a message if history is empty. |
| H | Change PIN | Verifies the current PIN, requires a valid four-digit new PIN and matching confirmation, then stores a newly salted hash. |
| I | Money transfer | Checks the recipient, same-account case, amount and sender balance. Debits the sender and credits the receiver in one validated operation and writes matching `TRANSFER_OUT`/`TRANSFER_IN` history rows. In-memory changes are rolled back if saving fails. |
| J | Logout / exit | Logout returns to the welcome menu; exit ends the program. Data is saved after every operation and once more on exit. |

### Bonus features included

- **Daily cash-withdrawal limit** of ₹25,000 (configurable).
- **Printable transaction receipt** after deposit, withdrawal and transfer.
- **Transaction reference numbers** such as `TXN-20261018223356-000002`.
- **Multiple accounts** with independent histories (the program supports any number of accounts).
- **Configurable maximum login attempts** (compile-time constant).
- **Automatic raw backup** of a CSV file when malformed rows are detected, so
  skipped data is not silently lost when the clean file is written.

### Optional bonus features intentionally not included

A full admin menu, account lookup by number and a complete (unlimited) history
report are not implemented. They were left out to keep the project focused on
the mandatory features and easy to explain in a viva. The mini statement is the
required five-transaction report.

---

## 3. Requirements

- **Language:** C++11 or later
- **Compiler:** MinGW g++ (tested with GCC 6.3.0) or any standard g++
- **Platform:** Windows (CMD/PowerShell/Windows Terminal) or Linux/macOS terminal
- **Libraries:** C++ standard library only
- **No database server, GUI framework or internet connection required**

The source is deliberately kept in **one file**, `main.cpp`, because that is
easiest to submit, compile and demonstrate for a first/second-year mini project.
It is organised into clearly labelled sections and classes.

### Project files

```text
PyBank_ATM_Simulator/
├── main.cpp                  Full C++ source (all classes and menus)
├── README.md                 This file
├── build.bat                 One-click Windows build helper
├── .gitignore
├── accounts.csv              Created on first run; stores accounts
├── transactions.csv          Created on first run; stores history
└── docs/
    ├── DESIGN.md             Class diagram, relationships and algorithms
    ├── SAMPLE_OUTPUT.md      Representative terminal transcripts
    ├── TEST_CASES.md         Test cases and actual results
    ├── SCREENSHOTS.md        Screenshots to capture for submission
    └── VIVA_QUESTIONS.md     Likely viva questions with simple answers
```

`accounts.csv` and `transactions.csv` do not need to exist at submission time.
They are created automatically in the **current working directory** on first run.

---

## 4. Compile and run (Windows / MinGW)

Open **Command Prompt** in the project folder:

```bat
cd PyBank_ATM_Simulator
g++ -std=c++11 -Wall -Wextra -pedantic main.cpp -o pybank_atm.exe
pybank_atm.exe
```

Or simply double-click/run **`build.bat`**, then run:

```bat
pybank_atm.exe
```

PowerShell:

```powershell
cd PyBank_ATM_Simulator
g++ -std=c++11 -Wall -Wextra -pedantic main.cpp -o pybank_atm.exe
.\pybank_atm.exe
```

Git Bash:

```bash
cd PyBank_ATM_Simulator
g++ -std=c++11 -Wall -Wextra -pedantic main.cpp -o pybank_atm.exe
./pybank_atm.exe
```

The current release was compiled and tested with **MinGW.org GCC 6.3.0 on
Windows 10/11** using the command above. It also compiles with `-Werror`.

### Linux / macOS

```bash
g++ -std=c++11 -Wall -Wextra -pedantic main.cpp -o pybank_atm
./pybank_atm
```

Run the executable from the project folder. The two CSV files are created or
loaded relative to the directory from which the program is launched.

### PIN masking note

On a native Windows console (`cmd.exe`, PowerShell or Windows Terminal) and on
Linux/macOS terminals, PIN entry is hidden and shown as `*`. The program falls
back to ordinary line input when stdin is redirected for automated testing; in
that mode the program does not echo the PIN itself, but a POSIX-style terminal
that redirects stdin (for example some Git Bash/mintty setups) may still echo
typed keystrokes. **Use CMD/PowerShell for the college demonstration** so the
masked input is clearly visible.

---

## 5. How to use

1. Run the program.
2. Choose **1. Create New Account** and enter a sample name, account number,
   four-digit PIN and opening deposit. The PIN is asked twice.
3. Choose **2. Login**, then enter the account number and PIN.
4. Use menu options 1–8 as needed. Deposits and withdrawals accept rupees with
   up to two decimal places (e.g. `500`, `500.5`, `500.50`). Press Enter at an
   amount prompt to cancel that operation.
5. Choose **7. Logout** to return to the welcome menu, or **8. Exit** to close
   the program. Both paths save the data safely.

Suggested demo accounts:

| Holder | Account number | PIN | Opening balance |
|--------|----------------|-----|-----------------|
| Aditya Malunjkar | `1000000001` | `1234` | ₹5,000.00 |
| Priya Sharma | `1000000002` | `5678` | ₹2,500.00 |

Do not use these (or any) credentials at a real bank.

---

## 6. Money handling

- Money is stored and calculated as **integer paise** (`long long`). ₹1 = 100 paise.
- Floating-point types are never used for balances or transactions, so there are
  no `0.1 + 0.2` rounding errors.
- Input is parsed manually. Signs, letters, commas, spaces inside the number and
  more than two decimal places are rejected.
- Balances and transaction amounts are always non-negative.
- Display uses the Indian grouping convention, e.g. `Rs. 1,23,456.78`.

The terminal displays **`Rs.`** rather than **₹** so the text is safe on older
Windows code pages. The currency is still Indian Rupees.

---

## 7. Local data files and CSV format

No external database is used. Both files are plain CSV with a header row.
CSV fields containing commas or quotes are quoted and embedded quotes are
doubled. Holder names are validated so they cannot contain commas; the parser and
writer still support quoted fields for safe future extensions. Newline characters
are not accepted in user input, so one record always occupies one CSV line.

### `accounts.csv`

```csv
account_no,name,pin_salt,pin_hash,balance_paise,created_at
1000000001,Aditya Malunjkar,bc77ecc2ea3de31020546b1de533853d,79fef49febe6c227,480025,2026-10-18 22:33:22
```

| Column | Meaning |
|--------|---------|
| `account_no` | 6–12 digit account number, stored as text so leading zeros survive |
| `name` | Account holder's full name |
| `pin_salt` | 32 random hex characters (16 bytes) |
| `pin_hash` | 16 hex characters: FNV-1a hash of `salt + ":" + PIN` |
| `balance_paise` | Non-negative integer balance in paise |
| `created_at` | Local date/time, `YYYY-MM-DD HH:MM:SS` |

**The plain PIN is never written to this file.**

### `transactions.csv`

```csv
account_no,date_time,type,amount_paise,balance_paise,reference,note
1000000001,2026-10-18 22:33:22,OPENING,500000,500000,TXN-20261018223322-000001,Opening deposit
1000000001,2026-10-18 22:33:22,DEPOSIT,50000,550000,TXN-20261018223322-000002,Cash deposit at PyBank ATM
```

| Column | Meaning |
|--------|---------|
| `account_no` | Owner of the transaction |
| `date_time` | Local date/time of the operation |
| `type` | `OPENING`, `DEPOSIT`, `WITHDRAWAL`, `TRANSFER_OUT` or `TRANSFER_IN` |
| `amount_paise` | Positive amount in paise |
| `balance_paise` | Balance **after** this transaction |
| `reference` | Human-readable transaction reference |
| `note` | Extra description, e.g. transfer counterparty |

Rows for all accounts share one file; the loader groups them by account and sorts
each account's history chronologically. A transfer writes two rows with the same
reference, one on each account.

### Saving behaviour

- Existing files are loaded at startup; the program never starts by blindly
  replacing them.
- Every successful operation is saved immediately.
- Files are first written to a `.tmp` file and then replaced, so a failed write
  is much less likely to destroy the previous file.
- If any invalid/skipped row is found while loading, the original raw file is
  copied to `accounts.csv.backup-YYYY-MM-DD-HH-MM-SS` (or the matching
  transactions backup) before the cleaned data can overwrite it.
- Missing or empty files are created with the correct header automatically.
- Malformed rows are reported and skipped; the rest of the data still loads.
  Orphan transactions (unknown account number) are skipped.
- Do not edit the files while the program is running, and do not use a
  spreadsheet that changes the date format. Make a copy before manual edits.

---

## 8. Login, limits and references

**Login attempts:** `MAX_LOGIN_ATTEMPTS` is defined at the top of `main.cpp`
(default 3). Failed attempts are consecutive across the current program run; a
successful login resets the counter. After the limit is reached, login is locked
until the program is restarted (account creation and exit remain available).
A wrong account number and a wrong PIN produce the same message, so the login
screen does not reveal whether an account exists.

**Daily withdrawal limit:** `DAILY_WITHDRAWAL_LIMIT_PAISE` is defined at the top
of `main.cpp` (default ₹25,000). It applies only to **cash withdrawals**
(`WITHDRAWAL`) made on the current local date. Transfers are not cash
withdrawals and are not counted. Set the constant to `0` to disable the limit.

**References:** references look like `TXN-20261018223356-000002`. They are
informational identifiers, not cryptographic tokens or globally registered bank
references. Before a new reference is used, the program checks all loaded history
to avoid duplicates within the local data set.

---

## 9. PIN protection and its limitations

New accounts receive a random 32-character salt. The stored value is an
FNV-1a hash of `salt + ":" + PIN`. Changing a PIN generates a new salt, so the
old hash cannot be reused.

This is **not** production-grade password security:

- A 4-digit PIN has only 10,000 possible values. Anyone who can read the CSV can
  brute-force the hash offline almost instantly.
- FNV-1a is a fast, non-cryptographic hash. It is not salted PBKDF2, bcrypt,
  scrypt or Argon2.
- The salt is stored next to the hash. Salting prevents two identical PINs from
  automatically producing the same stored hash, but it does not make a weak PIN
  strong.
- Files are ordinary local text files without access-control or encryption.
- The program does not implement encrypted transport, remote authentication,
  hardware security modules, audit servers, fraud detection or regulatory
  controls.

For a real system, use a slow password-hashing algorithm, strong authentication,
secure storage and a proper backend. For this classroom project, the salted hash
is enough to demonstrate that storing a bare PIN as plain text is unnecessary.

---

## 10. Validation summary

The program checks:

- empty input, letters where a number is expected and invalid menu choices;
- account numbers (6–12 digits), duplicate account numbers and missing accounts;
- names (2–50 characters; English letters, spaces, dots, apostrophes, hyphens;
  must begin and end with a letter);
- four-digit PINs, PIN confirmation, wrong current PIN and change-to-same-PIN;
- zero, negative, malformed, too-precise and oversized amounts;
- insufficient funds and daily-limit excess;
- transfers to the same account or a nonexistent account;
- three consecutive failed logins;
- missing/empty/malformed data files and unwritable files.

Failed validation never changes an in-memory balance. If writing to disk fails
during a successful operation, the account snapshot is restored and the error is
shown. There is still a theoretical crash window between replacing
`accounts.csv` and `transactions.csv`, because two separate files cannot be
committed as a single database transaction.

---

## 11. Limitations

- Educational simulator only; it cannot process real money.
- Single-machine, single-process application. There is no file locking, so two
  simultaneous copies should not use the same folder.
- PIN protection is simplified and offline-brute-forceable (see Section 9).
- The name validator accepts English letters and a small set of punctuation.
- Account recovery, admin management, statements beyond five rows, account
  search, interest, cheque/UPI features and multi-user concurrency are out of
  scope.
- Local clock values are used for date/time; changing the system clock affects
  the "today" daily-limit calculation.
- Saving uses two files; a sudden power loss between the two file replacements
  can theoretically leave them inconsistent. Regular backups are recommended.
- The terminal shows `Rs.` rather than the ₹ glyph for broad console support.

---

## 12. Documentation for submission and viva

- [Class design, relationships, function reference and algorithms](docs/DESIGN.md)
- [Sample terminal output](docs/SAMPLE_OUTPUT.md)
- [Test cases and actual results](docs/TEST_CASES.md)
- [Screenshot checklist](docs/SCREENSHOTS.md)
- [Likely viva questions with answers](docs/VIVA_QUESTIONS.md)

The program was built with `-Wall -Wextra -pedantic -Werror` and exercised with
normal operations, invalid input, wrong PINs, insufficient funds, duplicate
accounts, daily-limit boundaries, transfers, restarts, malformed files and a
simulated write failure. The detailed results are in `docs/TEST_CASES.md`.

---

## 13. Academic integrity / use

This project is provided as a learning reference for the OOPS mini-project
exercise. Understand the code before demonstrating it. Replace the sample
accounts and restart with your own data for screenshots. Do not present it as
production banking software or use it to handle real money or credentials.
