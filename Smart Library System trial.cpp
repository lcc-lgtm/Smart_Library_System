#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <map>
#include <fstream> // for file io stream
#include <filesystem> // safe export folder handling

using namespace std;
namespace fs = std::filesystem;

// clear console so only the current menu is visible
void clearScreen() {
#ifdef _WIN32
    system("cls");      // on Windows
#else
    system("clear");    // Linux/macOS
#endif
}

// Pauses after an action's output so the user can read it before the screen
// clears and the menu is redrawn
void pauseForUser() {
    cout << "\nPress Enter to continue...";
    cin.get();
}

// Formats a monetary amount as a fixed 2-decimal string (e.g. "12.50")
// used when building table cells where the value must sit inside a fixed-width column
string formatMoney(double amount) {
    ostringstream oss;
    oss << fixed << setprecision(2) << amount;
    return oss.str();
}

// ---------- Constants used across the system ----------

// Array capacity for each member's borrowed-book slots
// Must be big enough to hold the LARGEST per-category borrow limit below (Staff = 8)
const int MAX_BORROW_CAPACITY = 8;

// Category-based borrowing rules: 
// each member category has its own maximum number of books that can be borrowed at once
// and its own loan period
const int MAX_BORROW_STUDENT = 5;
const int MAX_BORROW_STAFF = 8;
const int MAX_BORROW_PUBLIC = 3;

const int LOAN_PERIOD_STUDENT_DAYS = 14;
const int LOAN_PERIOD_STAFF_DAYS = 21;
const int LOAN_PERIOD_PUBLIC_DAYS = 7;

const double FINE_RATE_PER_DAY = 0.50;
const double MAX_FINE = 20.00;

// ---------- Per-copy tracking constants ----------
const int MAX_BOOKS = 100;             // max number of distinct titles the system can ever register
const int MAX_COPIES_PER_BOOK = 10;    // max physical copies any single title can have


int getMaxBorrowLimit(const string& category) {
    if (category == "Staff")
        return MAX_BORROW_STAFF;
    if (category == "Public")
        return MAX_BORROW_PUBLIC;
    return MAX_BORROW_STUDENT; // default / Student
}

// Returns the loan period (in days) applicable to the given member category.
int getLoanPeriodDays(const string& category) {
    if (category == "Staff")
        return LOAN_PERIOD_STAFF_DAYS;
    if (category == "Public")
        return LOAN_PERIOD_PUBLIC_DAYS;
    return LOAN_PERIOD_STUDENT_DAYS; // default / Student
}

// A single borrowed book record, stored inside each Member
struct BorrowedBook {
    int bookID;
    int borrowDay;
};

struct Member {
    int memberID;
    string name;
    string category;              // Student / Staff / Public
    double fineBalance;
    BorrowedBook borrowedBooks[MAX_BORROW_CAPACITY];
    int borrowedCount;
};

struct Book {
    int bookID;
    string title;
    string author;
    string category;
    int totalCopies;
    int copiesAvailable;
    int copySlot;                 // permanent row index into copyBorrowed[][], assigned once at creation
};

struct Reservation {
    int memberID;
    int bookID;
};

// ---------- Input validation ----------
int getValidInt(string prompt, int minVal, int maxVal) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value && value >= minVal && value <= maxVal) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        cout << "Invalid input. Please enter an integer from " << minVal << " to " << maxVal << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

double getValidDouble(string prompt, double minVal) {
    double value;
    while (true) {
        cout << prompt;
        if (cin >> value && value >= minVal) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        cout << "Invalid input. Please enter a number >= " << fixed << setprecision(2) << minVal << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string getValidString(string prompt) {
    string value;
    while (true) {
        cout << prompt;
        getline(cin, value);
        if (!value.empty())
            return value;
        cout << "Input cannot be empty. Please try again.\n";
    }
}

// restrict member category to a fixed set of choices
string getMemberCategory() {
    cout << "\nMember Category:\n  1. Student\n  2. Staff\n  3. Public\n";
    int choice = getValidInt("Enter category    : ", 1, 3);
    if (choice == 1)
        return "Student";
    if (choice == 2)
        return "Staff";
    return "Public";
}

// ---------- Search helpers ----------
int findMemberIndex(const vector<Member>& members, int memberID) {
    for (int i = 0; i < static_cast<int>(members.size()); i++)
        if (members[i].memberID == memberID)
            return i;
    return -1;
}

int findBookIndex(const vector<Book>& books, int bookID) {
    for (int i = 0; i < static_cast<int>(books.size()); i++)
        if (books[i].bookID == bookID) return i;
    return -1;
}

// ---------- Fine calculation helpers ----------
double calculateEstimatedFine(const Member& member, int currentDay) {
    double estimated = 0.00;
    int loanPeriodDays = getLoanPeriodDays(member.category);
    for (int i = 0; i < member.borrowedCount; i++) {
        int overdueDays = (currentDay - member.borrowedBooks[i].borrowDay) - loanPeriodDays;
        if (overdueDays > 0) {
            double fine = overdueDays * FINE_RATE_PER_DAY;
            // cap applies per loan, same as returnBook()
            if (fine > MAX_FINE) fine = MAX_FINE;  
            
            estimated += fine;
        }
    }
    return estimated;
}

// the "true" fine picture for a member: fines already posted (fineBalance) plus
// fines still accruing on books that are overdue but not yet returned
double getTotalFine(const Member& member, int currentDay) {
    return member.fineBalance + calculateEstimatedFine(member, currentDay);
}

// ---------- Per-copy tracking helpers ----------
// copyBorrowed[slot][i] == true  -> that physical copy is currently on loan
// copyBorrowed[slot][i] == false -> that physical copy is on the shelf

// resets a book's row
void resetCopyRow(bool copyBorrowed[][MAX_COPIES_PER_BOOK], int slot, int totalCopies, int borrowedCount) {
    for (int i = 0; i < MAX_COPIES_PER_BOOK; i++) {
        copyBorrowed[slot][i] = (i < totalCopies) && (i < borrowedCount);
    }
}

// mark one available copy of this book as borrowed
int markOneCopyBorrowed(bool copyBorrowed[][MAX_COPIES_PER_BOOK], int slot, int totalCopies) {
    for (int i = 0; i < totalCopies && i < MAX_COPIES_PER_BOOK; i++) {
        if (!copyBorrowed[slot][i]) {
            copyBorrowed[slot][i] = true;
            return i + 1;
        }
    }
    return -1;
}

// mark one borrowed copy of this book as returned (back on the shelf)
int markOneCopyReturned(bool copyBorrowed[][MAX_COPIES_PER_BOOK], int slot, int totalCopies) {
    for (int i = 0; i < totalCopies && i < MAX_COPIES_PER_BOOK; i++) {
        if (copyBorrowed[slot][i]) {
            copyBorrowed[slot][i] = false;
            return i + 1;
        }
    }
    return -1;
}

// catalogue with course-based book titles
void initializeBooks(vector<Book>& books, bool copyBorrowed[][MAX_COPIES_PER_BOOK], int& nextBookSlot) {
    struct Seed {
        int id;
        string title;
        string category;
        int copies;
    };
    vector<Seed> seedData = {
        // Computer Science (7 books)
        {1,  "Computer Architecture",                        "Computer Science", 2},
        {2,  "Operating Systems",                             "Computer Science", 3},
        {3,  "Systems Analysis and Design",                   "Computer Science", 2},
        {4,  "Fundamentals of Artificial Intelligence",       "Computer Science", 3},
        {5,  "Mini Project",                                  "Computer Science", 2},
        {6,  "Parallel and Distributed Computing",            "Computer Science", 3},
        {7,  "Introduction to Cybersecurity",                 "Computer Science", 3},
        // Programming (7 books)
        {8,  "Problem Solving and Programming",              "Programming",      3},
        {9,  "Database Development and Applications",        "Programming",      3},
        {10, "Software Development Fundamentals",            "Programming",      4},
        {11, "Object-Oriented Programming Techniques",       "Programming",      4},
        {12, "Introduction to Data Structures and Algorithms","Programming",      4},
        {13, "Systems and Programming Concepts",              "Programming",      5},
        {14, "Mobile Application Development",                "Programming",      3},
        // General Studies (5 books)
        {15, "Integrity and Anti-Corruption",                "General Studies",  2},
        {16, "Penghayatan Etika dan Peradaban",               "General Studies",  2},
        {17, "Ethics in Computing",                           "General Studies",  2},
        {18, "Civic Consciousness and Volunteerism",          "General Studies",  2},
        {19, "Industrial Training",                           "General Studies",  1},
        // Language (3 books)
        {20, "English for Tertiary Studies",                 "Language",         2},
        {21, "Academic English",                              "Language",         2},
        {22, "Bahasa Kebangsaan A",                           "Language",         2},
        // Mathematics (3 books)
        {23, "Calculus and Algebra",                         "Mathematics",      3},
        {24, "Probability and Statistics",                   "Mathematics",      3},
        {25, "Discrete Mathematics",                          "Mathematics",      3},
        // Design (1 book)
        {26, "Introduction to Interface Design",             "Design",           2},
        // Networking (1 book)
        {27, "Fundamentals of Computer Networks",             "Networking",       3}
    };
    for (const auto& s : seedData) {
        Book b;
        b.bookID = s.id;
        b.title = s.title;
        b.author = "TAR UMT";
        b.category = s.category;
        b.totalCopies = s.copies;
        b.copiesAvailable = s.copies;
        b.copySlot = nextBookSlot++;
        resetCopyRow(copyBorrowed, b.copySlot, b.totalCopies, 0); // none borrowed yet
        books.push_back(b);
    }
}

void initializeMembers(vector<Member>& members) {
    struct Seed {
        int id;
        string name;
        string category;
    };
    vector<Seed> seedData = {};
    for (const auto& s : seedData) {
        Member m{};
        m.memberID = s.id;
        m.name = s.name;
        m.category = s.category;
        m.fineBalance = 0.00;
        m.borrowedCount = 0;
        for (int i = 0; i < MAX_BORROW_CAPACITY; i++) {
            m.borrowedBooks[i].bookID = 0;
            m.borrowedBooks[i].borrowDay = 0;
        }
        members.push_back(m);
    }
}

// ---------- Module 1: Member Management ----------
void addMember(vector<Member>& members) {
    cout << "\n===== Add Member =====\n";
    int memberID = getValidInt("Enter Member ID : ", 1, 999999999);
    if (findMemberIndex(members, memberID) != -1) {
        cout << "Member ID already exists.\n";
        return;
    }
    Member member{};
    member.memberID = memberID;
    member.name = getValidString("Enter name    : ");
    member.category = getMemberCategory();
    member.fineBalance = 0.00;
    member.borrowedCount = 0;
    for (int i = 0; i < MAX_BORROW_CAPACITY; i++) {
        member.borrowedBooks[i].bookID = 0;
        member.borrowedBooks[i].borrowDay = 0;
    }
    members.push_back(member);
    cout << "\nMember added successfully.\n";
    cout << "This member's borrowing limit is " << getMaxBorrowLimit(member.category)
        << " book(s), loan period " << getLoanPeriodDays(member.category) << " day(s).\n";
}

// Col width for the member table (used by both "View Members" and "Search Member"
const int COL_MEM_ID = 6;
const int COL_MEM_NAME = 20;
const int COL_MEM_CATEGORY = 10;
const int COL_MEM_RECORDED = 12;
const int COL_MEM_ESTIMATED = 12;
const int COL_MEM_TOTAL = 12;
const int COL_MEM_BORROWED = 10;
const int COL_MEM_LOAN = 12;

void printMemberTableHeader() {
    cout << left
        << setw(COL_MEM_ID) << "ID" << "| "
        << setw(COL_MEM_NAME) << "Name" << "| "
        << setw(COL_MEM_CATEGORY) << "Category" << "| "
        << setw(COL_MEM_RECORDED) << "Recorded" << "| "
        << setw(COL_MEM_ESTIMATED) << "Estimated" << "| "
        << setw(COL_MEM_TOTAL) << "Total Fine" << "| "
        << setw(COL_MEM_BORROWED) << "Borrowed" << "| "
        << setw(COL_MEM_LOAN) << "Loan Period" << "\n";
    int lineWidth = COL_MEM_ID + COL_MEM_NAME + COL_MEM_CATEGORY + COL_MEM_RECORDED
        + COL_MEM_ESTIMATED + COL_MEM_TOTAL + COL_MEM_BORROWED + COL_MEM_LOAN + (7 * 2); // 7 "| " separators
    cout << string(lineWidth, '-') << "\n";
}

void printMemberTableRow(const Member& member, int currentDay) {
    // truncate an unusually long name so it can never break the table alignment
    string name = member.name;
    if (static_cast<int>(name.length()) > COL_MEM_NAME - 1) {
        name = name.substr(0, COL_MEM_NAME - 4) + "...";
    }
    double estimatedFine = calculateEstimatedFine(member, currentDay);
    string recordedCell = "RM " + formatMoney(member.fineBalance);
    string estimatedCell = "RM " + formatMoney(estimatedFine);
    string totalCell = "RM " + formatMoney(member.fineBalance + estimatedFine);
    string borrowedCell = to_string(member.borrowedCount) + "/" + to_string(getMaxBorrowLimit(member.category));
    string loanCell = to_string(getLoanPeriodDays(member.category)) + " day(s)";
    cout << left
        << setw(COL_MEM_ID) << member.memberID << "| "
        << setw(COL_MEM_NAME) << name << "| "
        << setw(COL_MEM_CATEGORY) << member.category << "| "
        << setw(COL_MEM_RECORDED) << recordedCell << "| "
        << setw(COL_MEM_ESTIMATED) << estimatedCell << "| "
        << setw(COL_MEM_TOTAL) << totalCell << "| "
        << setw(COL_MEM_BORROWED) << borrowedCell << "| "
        << setw(COL_MEM_LOAN) << loanCell << "\n";
}

void viewMembers(const vector<Member>& members, int currentDay) {
    cout << "\n===== Member List =====\n";
    if (members.empty()) { cout << "No members found.\n"; return; }
    printMemberTableHeader();
    for (const Member& member : members) printMemberTableRow(member, currentDay);
}

// search members by ID or name using a regex pattern
void searchMember(const vector<Member>& members, int currentDay) {
    cout << "\n===== Search Member =====\n";
    if (members.empty()) { cout << "No members found.\n"; return; }

    string keyword = getValidString("Enter Member Name or ID    : ");

    regex pattern;
    try {
        // user input as a regex
        pattern = regex(keyword, regex::icase);
    }
    catch (const regex_error&) {
        cout << "Invalid search pattern. Please avoid unsupported regex syntax.\n";
        return;
    }

    // traverse the member vector and collect every match into a new vector
    vector<Member> searchResults;
    for (const Member& member : members) {
        string idText = to_string(member.memberID);
        if (regex_search(idText, pattern) || regex_search(member.name, pattern)) {
            searchResults.push_back(member);
        }
    }

    if (searchResults.empty()) {
        cout << "\nNo matching member(s) found.\n";
        return;
    }

    cout << "\nFound " << searchResults.size() << " matching member(s):\n\n";
    printMemberTableHeader();
    for (const Member& member : searchResults) {
        printMemberTableRow(member, currentDay);
    }

    // clear the results vector now that it has been displayed
    searchResults.clear();
}

void updateMember(vector<Member>& members) {
    cout << "\n===== Update Member =====\n";
    
    int memberID = getValidInt("Enter Member ID: ", 1, 999999999);
    int index = findMemberIndex(members, memberID);
    
    if (index == -1) { cout << "Member not found.\n"; return; }

    string newCategory = getMemberCategory();
   
    if (members[index].borrowedCount > getMaxBorrowLimit(newCategory)) {
        cout << "Cannot change category.\nThis member currently holds " << members[index].borrowedCount
            << " book(s), which exceeds the " << newCategory << " limit of "
            << getMaxBorrowLimit(newCategory) << ".\n";
        return;
    }
    members[index].name = getValidString("Enter new name: ");
    members[index].category = newCategory;
    cout << "Member updated successfully.\n";
}

void deleteMember(vector<Member>& members, const vector<Reservation>& reservations) {
    cout << "\n===== Delete Member =====\n";
    
    int memberID = getValidInt("Enter Member ID: ", 1, 999999999);
    int index = findMemberIndex(members, memberID);
    
    if (index == -1) { cout << "Member not found.\n"; return; }
    if (members[index].borrowedCount != 0) {
        cout << "Cannot delete member.\nThe member still has borrowed books.\n";
        return;
    }
    // block deletion if the member is still waiting in a reservation queue
    for (const Reservation& reservation : reservations) {
        if (reservation.memberID == memberID) {
            cout << "Cannot delete member.\nThis member has pending reservation(s).\n";
            return;
        }
    }
    members.erase(members.begin() + index);
    cout << "Member deleted successfully.\n";
}

// ---------- Module 1: Book Catalogue Management ----------
void addBook(vector<Book>& books, bool copyBorrowed[][MAX_COPIES_PER_BOOK], int& nextBookSlot) {
    cout << "\n===== Add Book =====\n";
    
    if (nextBookSlot >= MAX_BOOKS) {
        cout << "Cannot add book.\nThe system has reached its maximum of " << MAX_BOOKS << " distinct titles.\n";
        return;
    }
    
    int bookID = getValidInt("Enter Book ID: ", 1, 999999999);
    
    if (findBookIndex(books, bookID) != -1) { cout << "Book ID already exists.\n"; return; }
    
    Book book{};
    book.bookID = bookID;
    book.title = getValidString("Enter title: ");
    book.author = getValidString("Enter author: ");
    book.category = getValidString("Enter category: ");
    
    // the total copies is capped at MAX_COPIES_PER_BOOK since each copy needs its own
    // tracked slot in the per-copy status matrix
    book.totalCopies = getValidInt(
        "Enter total copies (max " + to_string(MAX_COPIES_PER_BOOK) + "): ",
        1, 
        MAX_COPIES_PER_BOOK
    );
    book.copiesAvailable = book.totalCopies;
    book.copySlot = nextBookSlot++;
    
    // brand new book, nothing borrowed yet
    resetCopyRow(copyBorrowed, book.copySlot, book.totalCopies, 0); 
    
    books.push_back(book);
    
    cout << "Book added successfully.\n";
}

// Col width for the book catalogue table
const int COL_BOOK_ID = 5;
const int COL_BOOK_TITLE = 50;
const int COL_BOOK_AUTHOR = 12;
const int COL_BOOK_CATEGORY = 18;
const int COL_BOOK_TOTAL = 7;
const int COL_BOOK_AVAILABLE = 9;

void printBookTableHeader() {
    cout << left
        << setw(COL_BOOK_ID) << "ID" << "| "
        << setw(COL_BOOK_TITLE) << "Title" << "| "
        << setw(COL_BOOK_AUTHOR) << "Author" << "| "
        << setw(COL_BOOK_CATEGORY) << "Category" << "| "
        << setw(COL_BOOK_TOTAL) << "Total" << "| "
        << setw(COL_BOOK_AVAILABLE) << "Available" << "\n";
    int lineWidth = COL_BOOK_ID + COL_BOOK_TITLE + COL_BOOK_AUTHOR + COL_BOOK_CATEGORY
        + COL_BOOK_TOTAL + COL_BOOK_AVAILABLE + (5 * 2);
    cout << string(lineWidth, '-') << "\n";
}

void printBookTableRow(const Book& book) {
    string title = book.title;
    if (static_cast<int>(title.length()) > COL_BOOK_TITLE - 1) {
        title = title.substr(0, COL_BOOK_TITLE - 4) + "...";
    }
    cout << left
        << setw(COL_BOOK_ID) << book.bookID << "| "
        << setw(COL_BOOK_TITLE) << title << "| "
        << setw(COL_BOOK_AUTHOR) << book.author << "| "
        << setw(COL_BOOK_CATEGORY) << book.category << "| "
        << setw(COL_BOOK_TOTAL) << book.totalCopies << "| "
        << setw(COL_BOOK_AVAILABLE) << book.copiesAvailable << "\n";
}

void viewBooks(const vector<Book>& books) {
    cout << "\n===== Book Catalogue =====\n";
    if (books.empty()) { cout << "No books found.\n"; return; }

    // group books by category 
    map<string, int> categoryCount;
    for (const Book& book : books) categoryCount[book.category]++;

    vector<Book> sortedBooks = books;
    stable_sort(
        sortedBooks.begin(), sortedBooks.end(), 

        [&categoryCount](const Book& a, const Book& b) {
            int countA = categoryCount[a.category];
            int countB = categoryCount[b.category];

            // bigger categories first
            if (countA != countB) return countA > countB;      
        
            // keep same category adjacent
            if (a.category != b.category) return a.category < b.category; 
            
            // stable order within a category
            return a.bookID < b.bookID;                          
            }
    );

    printBookTableHeader();
    string lastCategory = "";
    for (const Book& book : sortedBooks) {
        // blank line between category groups makes the grouping visually obvious
        if (!lastCategory.empty() && book.category != lastCategory) cout << "\n";
        
        lastCategory = book.category;
        
        printBookTableRow(book);
    }
}

void searchBook(const vector<Book>& books) {
    cout << "\n===== Search Book =====\n";
    
    int bookID = getValidInt("Enter Book ID: ", 1, 999999999);
    int index = findBookIndex(books, bookID);
    
    if (index == -1) { cout << "Book not found.\n"; return; }
    
    cout << "\n";
    
    printBookTableHeader();
    printBookTableRow(books[index]);
}

void updateBook(vector<Book>& books, bool copyBorrowed[][MAX_COPIES_PER_BOOK]) {
    cout << "\n===== Update Book =====\n";
    
    int bookID = getValidInt("Enter Book ID: ", 1, 999999999);
    int index = findBookIndex(books, bookID);
    
    if (index == -1) { cout << "Book not found.\n"; return; }
    
    Book& book = books[index];
    book.title = getValidString("Enter new title: ");
    book.author = getValidString("Enter new author: ");
    book.category = getValidString("Enter new category: ");

    // new total cannot go below copies currently on loan
    // & cannot exceed the per-book slot capacity of the copy-tracking matrix
    int borrowedCopies = book.totalCopies - book.copiesAvailable;
    
    int newTotalCopies = getValidInt(
        "Enter new total copies (" + to_string(borrowedCopies) + "-" + to_string(MAX_COPIES_PER_BOOK) + "): ",
        borrowedCopies, MAX_COPIES_PER_BOOK);
    
    book.totalCopies = newTotalCopies;
    book.copiesAvailable = newTotalCopies - borrowedCopies;
    
    resetCopyRow(copyBorrowed, book.copySlot, newTotalCopies, borrowedCopies);
    
    cout << "Book updated successfully.\n";
}

void deleteBook(vector<Book>& books, const vector<Reservation>& reservations) {
    cout << "\n===== Delete Book =====\n";
    int bookID = getValidInt("Enter Book ID: ", 1, 999999999);
    int index = findBookIndex(books, bookID);
    if (index == -1) { cout << "Book not found.\n"; return; }
    if (books[index].copiesAvailable != books[index].totalCopies) {
        cout << "Cannot delete book.\nSome copies are currently borrowed.\n";
        return;
    }
    // block deletion if someone is still waiting for this book
    for (const Reservation& reservation : reservations) {
        if (reservation.bookID == bookID) {
            cout << "Cannot delete book.\nThis book has pending reservation(s).\n";
            return;
        }
    }
    books.erase(books.begin() + index);
    cout << "Book deleted successfully.\n";
}

// Col width for per-copy status table
const int COL_COPY_NUMBER = 6;
const int COL_COPY_STATUS = 10;

void viewBookCopyStatus(const vector<Book>& books, const bool copyBorrowed[][MAX_COPIES_PER_BOOK]) {
    cout << "\n===== View Book Copy Status =====\n";
    int bookID = getValidInt("Enter Book ID: ", 1, 999999999);
    int index = findBookIndex(books, bookID);
    if (index == -1) { cout << "Book not found.\n"; return; }
    const Book& book = books[index];

    cout << "\n";
    printBookTableHeader();
    printBookTableRow(book);

    cout << "\n";
    cout << left
        << setw(COL_COPY_NUMBER) << "Copy" << "| "
        << setw(COL_COPY_STATUS) << "Status" << "\n";
    cout << string(COL_COPY_NUMBER + COL_COPY_STATUS + 2, '-') << "\n";
    for (int i = 0; i < book.totalCopies; i++) {
        cout << left
            << setw(COL_COPY_NUMBER) << (i + 1) << "| "
            << setw(COL_COPY_STATUS) << (copyBorrowed[book.copySlot][i] ? "BORROWED" : "On Shelf") << "\n";
    }
}

// ---------- Module 2: Reservation Management ----------
void reserveBook(vector<Member>& members, vector<Book>& books, vector<Reservation>& reservations) {
    cout << "\n===== Reserve Book =====\n";
    int memberID = getValidInt("Enter Member ID: ", 1, 999999999);
    int memberIndex = findMemberIndex(members, memberID);
    if (memberIndex == -1) { cout << "Member not found.\n"; return; }
    int bookID = getValidInt("Enter Book ID: ", 1, 999999999);
    int bookIndex = findBookIndex(books, bookID);
    if (bookIndex == -1) { cout << "Book not found.\n"; return; }
    Book& book = books[bookIndex];
    if (book.copiesAvailable > 0) {
        cout << "Book is currently available.\nYou can borrow it directly instead of reserving it.\n";
        return;
    }
    for (const Reservation& reservation : reservations) {
        if (reservation.memberID == memberID && reservation.bookID == bookID) {
            cout << "You already reserved this book.\n";
            return;
        }
    }
    Reservation reservation;
    reservation.memberID = memberID;
    reservation.bookID = bookID;
    reservations.push_back(reservation); 
    cout << "Reservation added successfully.\nYour reservation is placed at the end of the FIFO queue.\n";
}

// Col width for the reservation queue table
const int COL_RES_POS = 6;
const int COL_RES_MEM_ID = 6;
const int COL_RES_MEM_NAME = 20;
const int COL_RES_BOOK_ID = 6;
const int COL_RES_BOOK_TITLE = 35;

void viewReservations(const vector<Member>& members, const vector<Book>& books, const vector<Reservation>& reservations) {
    cout << "\n===== Reservation List =====\n";
    if (reservations.empty()) { cout << "No reservations found.\n"; return; }

    cout << left
        << setw(COL_RES_POS) << "Pos" << "| "
        << setw(COL_RES_MEM_ID) << "Mem ID" << "| "
        << setw(COL_RES_MEM_NAME) << "Member Name" << "| "
        << setw(COL_RES_BOOK_ID) << "BookID" << "| "
        << setw(COL_RES_BOOK_TITLE) << "Book Title" << "\n";
    int lineWidth = COL_RES_POS + COL_RES_MEM_ID + COL_RES_MEM_NAME + COL_RES_BOOK_ID + COL_RES_BOOK_TITLE + (4 * 2); // 4 "| " separators
    cout << string(lineWidth, '-') << "\n";

    for (int i = 0; i < static_cast<int>(reservations.size()); i++) {
        const Reservation& reservation = reservations[i];
        int memberIndex = findMemberIndex(members, reservation.memberID);
        int bookIndex = findBookIndex(books, reservation.bookID);

        string memberName = (memberIndex != -1) ? members[memberIndex].name : "(unknown member)";
        if (static_cast<int>(memberName.length()) > COL_RES_MEM_NAME - 1) {
            memberName = memberName.substr(0, COL_RES_MEM_NAME - 4) + "...";
        }
        string bookTitle = (bookIndex != -1) ? books[bookIndex].title : "(unknown book)";
        if (static_cast<int>(bookTitle.length()) > COL_RES_BOOK_TITLE - 1) {
            bookTitle = bookTitle.substr(0, COL_RES_BOOK_TITLE - 4) + "...";
        }

        cout << left
            << setw(COL_RES_POS) << (i + 1) << "| "
            << setw(COL_RES_MEM_ID) << reservation.memberID << "| "
            << setw(COL_RES_MEM_NAME) << memberName << "| "
            << setw(COL_RES_BOOK_ID) << reservation.bookID << "| "
            << setw(COL_RES_BOOK_TITLE) << bookTitle << "\n";
    }
}

void cancelReservation(vector<Member>& members, vector<Book>& books, vector<Reservation>& reservations) {
    cout << "\n===== Cancel Reservation =====\n";
    int memberID = getValidInt("Enter Member ID: ", 1, 999999999);
    int memberIndex = findMemberIndex(members, memberID);
    if (memberIndex == -1) { cout << "Member not found.\n"; return; }
    int bookID = getValidInt("Enter Book ID: ", 1, 999999999);
    int bookIndex = findBookIndex(books, bookID);
    if (bookIndex == -1) { cout << "Book not found.\n"; return; }
    for (int i = 0; i < static_cast<int>(reservations.size()); i++) {
        if (reservations[i].memberID == memberID && reservations[i].bookID == bookID) {
            reservations.erase(reservations.begin() + i);
            cout << "Reservation cancelled successfully.\n";
            return;
        }
    }
    cout << "Reservation not found.\n";
}

// ---------- Module 3: Borrowing & Returning ----------
void borrowBook(vector<Member>& members, vector<Book>& books, vector<Reservation>& reservations, int& currentDay,
    bool copyBorrowed[][MAX_COPIES_PER_BOOK]) {
    cout << "\n===== Borrow Book =====\n";
    int memberID = getValidInt("Enter Member ID: ", 1, 999999999);
    int memberIndex = findMemberIndex(members, memberID);
    if (memberIndex == -1) { cout << "Member not found.\n"; return; }
    Member& member = members[memberIndex];
    double totalFine = getTotalFine(member, currentDay);
    if (totalFine > 0) {
        cout << "Cannot borrow books.\nOutstanding fine: RM " << fixed << setprecision(2) << totalFine;
        if (member.fineBalance < totalFine - 0.005) {
            cout << " (includes RM " << (totalFine - member.fineBalance)
                << " still accruing on an overdue book that hasn't been returned yet)";
        }
        cout << "\n";
        return;
    }
    // borrowing limit depends on member's category (Student/Staff/Public)
    int maxBorrowLimit = getMaxBorrowLimit(member.category);
    if (member.borrowedCount >= maxBorrowLimit) {
        cout << "Cannot borrow more books.\nMaximum borrowing limit for " << member.category
            << " members is " << maxBorrowLimit << " book(s).\n";
        return;
    }
    // show catalogue to see what is available before picking an ID.
    viewBooks(books);
    int bookID = getValidInt("\nEnter Book ID: ", 1, 999999999);
    int bookIndex = findBookIndex(books, bookID);
    if (bookIndex == -1) { cout << "Book not found.\n"; return; }

    // same member cannot hold two copies of the same book
    for (int i = 0; i < member.borrowedCount; i++) {
        if (member.borrowedBooks[i].bookID == bookID) {
            cout << "You have already borrowed this book.\nThe same member cannot borrow the same book twice.\n";
            return;
        }
    }

    Book& book = books[bookIndex];
    if (book.copiesAvailable <= 0) {
        cout << "No copies are currently available.\nPlease make a reservation instead.\n";
        return;
    }

    // if this book has a reservation queue, only the first person in line
    int reservationIndex = -1;
    for (int i = 0; i < static_cast<int>(reservations.size()); i++) {
        if (reservations[i].bookID == bookID) { reservationIndex = i; break; }
    }
    if (reservationIndex != -1) {
        if (reservations[reservationIndex].memberID != memberID) {
            cout << "This book has a reservation queue.\nThe current borrower is not the first member in the queue.\n";
            return;
        }
        reservations.erase(reservations.begin() + reservationIndex);
    }

    member.borrowedBooks[member.borrowedCount].bookID = bookID;
    member.borrowedBooks[member.borrowedCount].borrowDay = currentDay;
    member.borrowedCount++;
    book.copiesAvailable--;
    
    int copyNumber = markOneCopyBorrowed(copyBorrowed, book.copySlot, book.totalCopies);
    int loanPeriodDays = getLoanPeriodDays(member.category);

    cout << "Book borrowed successfully.\n";
    if (copyNumber != -1) cout << "Physical Copy Assigned: Copy " << copyNumber << " of " << book.totalCopies << "\n";
    cout << "Borrow Day: " << currentDay << "\nDue Day: " << currentDay + loanPeriodDays
        << " (" << member.category << " loan period: " << loanPeriodDays << " day(s))\n";
}

void returnBook(vector<Member>& members, vector<Book>& books, vector<Reservation>& reservations, int& currentDay,
    bool copyBorrowed[][MAX_COPIES_PER_BOOK]) {
    cout << "\n===== Return Book =====\n";
    int memberID = getValidInt("Enter Member ID: ", 1, 999999999);
    int memberIndex = findMemberIndex(members, memberID);
    if (memberIndex == -1) { cout << "Member not found.\n"; return; }
    Member& member = members[memberIndex];
    if (member.borrowedCount == 0) { cout << "This member has no borrowed books.\n"; return; }
    int bookID = getValidInt("Enter Book ID: ", 1, 999999999);
    int borrowedIndex = -1;
    for (int i = 0; i < member.borrowedCount; i++) {
        if (member.borrowedBooks[i].bookID == bookID) { borrowedIndex = i; break; }
    }
    if (borrowedIndex == -1) { cout << "This member did not borrow this book.\n"; return; }

    int borrowDay = member.borrowedBooks[borrowedIndex].borrowDay;
    // overdue calculation now uses this member's own loan period (based on category)
    int loanPeriodDays = getLoanPeriodDays(member.category);
    int overdueDays = (currentDay - borrowDay) - loanPeriodDays;
    if (overdueDays > 0) {
        double fine = overdueDays * FINE_RATE_PER_DAY;
        if (fine > MAX_FINE) fine = MAX_FINE;
        member.fineBalance += fine;
        cout << "Book is overdue by " << overdueDays << " day(s).\nFine charged: RM " << fixed << setprecision(2) << fine << "\n";
    }
    else {
        cout << "Book returned on time.\n";
    }

    // shift remaining entries left to fill the gap left by the returned book
    for (int i = borrowedIndex; i < member.borrowedCount - 1; i++) member.borrowedBooks[i] = member.borrowedBooks[i + 1];
    member.borrowedCount--;
    member.borrowedBooks[member.borrowedCount].bookID = 0;
    member.borrowedBooks[member.borrowedCount].borrowDay = 0;

    int bookIndex = findBookIndex(books, bookID);
    if (bookIndex != -1) {
        books[bookIndex].copiesAvailable++;
        // only exactly one physical copy of this title can put back on the shelf
        int copyNumber = markOneCopyReturned(copyBorrowed, books[bookIndex].copySlot, books[bookIndex].totalCopies);
        cout << "Book returned successfully.\n";
        if (copyNumber != -1) cout << "Physical Copy Returned: Copy " << copyNumber << " of " << books[bookIndex].totalCopies << "\n";

        // Notify the next member in reservation queue
        bool hasReservation = false;
        for (const Reservation& reservation : reservations) {
            if (reservation.bookID == bookID) {
                hasReservation = true;
                int reserveMemberIndex = findMemberIndex(members, reservation.memberID);
                cout << "\n*** Reservation Notification ***\n";
                if (reserveMemberIndex != -1)
                    cout << "Member " << reservation.memberID << " (" << members[reserveMemberIndex].name << ") is waiting for this book.\n";
                else
                    cout << "Member " << reservation.memberID << " is waiting for this book.\n";
                cout << "Book ID: " << bookID << "\n";
                break;
            }
        }
        if (!hasReservation) cout << "There are no reservations for this book.\n";
    }
}

// Col width for the "member's borrowed books" table
const int COL_LOAN_ID = 5;
const int COL_LOAN_TITLE = 35;
const int COL_LOAN_BORROW_DAY = 11;
const int COL_LOAN_DUE_DAY = 9;
const int COL_LOAN_STATUS = 11;
const int COL_LOAN_OVERDUE = 13;

void viewBorrowedBooks(const vector<Member>& members, const vector<Book>& books, int currentDay) {
    cout << "\n===== View Member's Borrowed Books =====\n";
    int memberID = getValidInt("Enter Member ID: ", 1, 999999999);
    int memberIndex = findMemberIndex(members, memberID);
    if (memberIndex == -1) { cout << "Member not found.\n"; return; }
    const Member& member = members[memberIndex];
    if (member.borrowedCount == 0) { cout << "This member has no borrowed books.\n"; return; }
    int loanPeriodDays = getLoanPeriodDays(member.category);
    cout << "\nMember: " << member.name << " (" << member.category << ")"
        << "\nBorrowed Books: " << member.borrowedCount << "/" << getMaxBorrowLimit(member.category)
        << "\nLoan Period: " << loanPeriodDays << " day(s)\n\n";

    cout << left
        << setw(COL_LOAN_ID) << "ID" << "| "
        << setw(COL_LOAN_TITLE) << "Title" << "| "
        << setw(COL_LOAN_BORROW_DAY) << "Borrow Day" << "| "
        << setw(COL_LOAN_DUE_DAY) << "Due Day" << "| "
        << setw(COL_LOAN_STATUS) << "Status" << "| "
        << setw(COL_LOAN_OVERDUE) << "Overdue Days" << "\n";
    int lineWidth = COL_LOAN_ID + COL_LOAN_TITLE + COL_LOAN_BORROW_DAY + COL_LOAN_DUE_DAY
        + COL_LOAN_STATUS + COL_LOAN_OVERDUE + (5 * 2);
    cout << string(lineWidth, '-') << "\n";

    double estimatedFine = 0.00;
    for (int i = 0; i < member.borrowedCount; i++) {
        int bookID = member.borrowedBooks[i].bookID;
        int borrowDay = member.borrowedBooks[i].borrowDay;
        int dueDay = borrowDay + loanPeriodDays;
        int bookIndex = findBookIndex(books, bookID);

        string title = (bookIndex != -1) ? books[bookIndex].title : "(unknown book)";
        if (static_cast<int>(title.length()) > COL_LOAN_TITLE - 1) {
            title = title.substr(0, COL_LOAN_TITLE - 4) + "...";
        }

        int overdueDays = (currentDay - borrowDay) - loanPeriodDays;
        string statusCell = (overdueDays > 0) ? "OVERDUE" : "OK";
        string overdueCell = "-";
        if (overdueDays > 0) {
            overdueCell = to_string(overdueDays) + " day(s)";
            double fine = overdueDays * FINE_RATE_PER_DAY;
            if (fine > MAX_FINE) fine = MAX_FINE;
            estimatedFine += fine;
        }

        cout << left
            << setw(COL_LOAN_ID) << bookID << "| "
            << setw(COL_LOAN_TITLE) << title << "| "
            << setw(COL_LOAN_BORROW_DAY) << borrowDay << "| "
            << setw(COL_LOAN_DUE_DAY) << dueDay << "| "
            << setw(COL_LOAN_STATUS) << statusCell << "| "
            << setw(COL_LOAN_OVERDUE) << overdueCell << "\n";
    }

    if (estimatedFine > 0) {
        cout << "\nEstimated fine if returned today (not yet posted to account): RM "
            << fixed << setprecision(2) << estimatedFine << "\n";
    }
}

// simulate the passage of time for overdue/fine testing
void advanceDay(int& currentDay) {
    cout << "\n===== Advance Day =====\n";
    int days = getValidInt("Enter number of days to advance: ", 1, 999999999);
    currentDay += days;
    cout << "Current simulated day is now: " << currentDay << "\n";
}

// ---------- Module 4: Fine Calculation & Reporting ----------
// Col width for the table shown by "View / Pay Fine"
const int COL_FINE_NAME = 20;
const int COL_FINE_RECORDED = 14;
const int COL_FINE_ESTIMATED = 14;
const int COL_FINE_TOTAL = 14;

void viewPayFine(vector<Member>& members, int currentDay) {
    cout << "\n===== View / Pay Fine =====\n";
    int memberID = getValidInt("Enter Member ID: ", 1, 999999999);
    int index = findMemberIndex(members, memberID);
    if (index == -1) { cout << "Member not found.\n"; return; }
    Member& member = members[index];
    double estimatedFine = calculateEstimatedFine(member, currentDay);
    double totalFine = member.fineBalance + estimatedFine;

    string name = member.name;
    if (static_cast<int>(name.length()) > COL_FINE_NAME - 1) {
        name = name.substr(0, COL_FINE_NAME - 4) + "...";
    }
    cout << "\n" << left
        << setw(COL_FINE_NAME) << "Name" << "| "
        << setw(COL_FINE_RECORDED) << "Recorded" << "| "
        << setw(COL_FINE_ESTIMATED) << "Estimated" << "| "
        << setw(COL_FINE_TOTAL) << "Total" << "\n";
    cout << string(COL_FINE_NAME + COL_FINE_RECORDED + COL_FINE_ESTIMATED + COL_FINE_TOTAL + (3 * 2), '-') << "\n";
    cout << left
        << setw(COL_FINE_NAME) << name << "| "
        << setw(COL_FINE_RECORDED) << ("RM " + formatMoney(member.fineBalance)) << "| "
        << setw(COL_FINE_ESTIMATED) << ("RM " + formatMoney(estimatedFine)) << "| "
        << setw(COL_FINE_TOTAL) << ("RM " + formatMoney(totalFine)) << "\n";

    if (estimatedFine > 0) {
        cout << "\nNote: the Estimated column is not finalized and cannot be paid yet - "
            << "it will be posted to Recorded (and may keep growing) once the overdue book is returned.\n";
    }
    if (member.fineBalance <= 0) { cout << "No payable (recorded) fine right now.\n"; return; }
    double payment = getValidDouble("\nEnter payment amount: RM ", 0.01);
    if (payment > member.fineBalance) { cout << "Payment cannot exceed the recorded fine.\n"; return; }
    member.fineBalance -= payment;
    if (member.fineBalance < 0.005) member.fineBalance = 0.00;  
    cout << "Payment successful.\nRemaining Recorded Fine: RM " << fixed << setprecision(2) << member.fineBalance << "\n";
}

// Col width for the overdue books report table
const int COL_OD_MEM_ID = 6;
const int COL_OD_MEM_NAME = 18;
const int COL_OD_CATEGORY = 9;
const int COL_OD_BOOK_ID = 7;
const int COL_OD_TITLE = 28;
const int COL_OD_DUE_DAY = 8;
const int COL_OD_OVERDUE = 10;
const int COL_OD_FINE = 10;

void overdueBooksReport(const vector<Member>& members, const vector<Book>& books, int currentDay) {
    cout << "\n===== Overdue Books Report =====\n";

    cout << left
        << setw(COL_OD_MEM_ID) << "MemID" << "| "
        << setw(COL_OD_MEM_NAME) << "Member Name" << "| "
        << setw(COL_OD_CATEGORY) << "Category" << "| "
        << setw(COL_OD_BOOK_ID) << "BookID" << "| "
        << setw(COL_OD_TITLE) << "Title" << "| "
        << setw(COL_OD_DUE_DAY) << "Due Day" << "| "
        << setw(COL_OD_OVERDUE) << "Overdue" << "| "
        << setw(COL_OD_FINE) << "Fine (RM)" << "\n";
    int lineWidth = COL_OD_MEM_ID + COL_OD_MEM_NAME + COL_OD_CATEGORY + COL_OD_BOOK_ID
        + COL_OD_TITLE + COL_OD_DUE_DAY + COL_OD_OVERDUE + COL_OD_FINE + (7 * 2); // 7 "| " separators
    cout << string(lineWidth, '-') << "\n";

    bool found = false;
    for (const Member& member : members) {
        int loanPeriodDays = getLoanPeriodDays(member.category);
        for (int i = 0; i < member.borrowedCount; i++) {
            int bookID = member.borrowedBooks[i].bookID;
            int borrowDay = member.borrowedBooks[i].borrowDay;
            int overdueDays = (currentDay - borrowDay) - loanPeriodDays;
            if (overdueDays > 0) {
                found = true;
                int bookIndex = findBookIndex(books, bookID);

                string memberName = member.name;
                if (static_cast<int>(memberName.length()) > COL_OD_MEM_NAME - 1) {
                    memberName = memberName.substr(0, COL_OD_MEM_NAME - 4) + "...";
                }
                string title = (bookIndex != -1) ? books[bookIndex].title : "(unknown book)";
                if (static_cast<int>(title.length()) > COL_OD_TITLE - 1) {
                    title = title.substr(0, COL_OD_TITLE - 4) + "...";
                }
                double fine = overdueDays * FINE_RATE_PER_DAY;
                if (fine > MAX_FINE) fine = MAX_FINE;

                cout << left
                    << setw(COL_OD_MEM_ID) << member.memberID << "| "
                    << setw(COL_OD_MEM_NAME) << memberName << "| "
                    << setw(COL_OD_CATEGORY) << member.category << "| "
                    << setw(COL_OD_BOOK_ID) << bookID << "| "
                    << setw(COL_OD_TITLE) << title << "| "
                    << setw(COL_OD_DUE_DAY) << (borrowDay + loanPeriodDays) << "| "
                    << setw(COL_OD_OVERDUE) << (to_string(overdueDays) + "d") << "| "
                    << setw(COL_OD_FINE) << formatMoney(fine) << "\n";
            }
        }
    }
    if (!found) cout << "No overdue books found.\n";
}

// Col width for the book popularity report table
const int COL_POP_RANK = 6;
const int COL_POP_ID = 6;
const int COL_POP_TITLE = 35;
const int COL_POP_AUTHOR = 10;
const int COL_POP_BORROWED = 12;
const int COL_POP_TOTAL = 7;

void bookPopularityReport(const vector<Book>& books) {
    cout << "\n===== Book Popularity Report =====\n";
    if (books.empty()) { cout << "No books found.\n"; return; }
    vector<Book> sortedBooks = books;
    // Rank by number of copies currently on loan (more higher more popular)
    sort(sortedBooks.begin(), sortedBooks.end(), [](const Book& a, const Book& b) {
        int borrowedA = a.totalCopies - a.copiesAvailable;
        int borrowedB = b.totalCopies - b.copiesAvailable;
        return borrowedA > borrowedB;
        });
    int limit = min(5, static_cast<int>(sortedBooks.size()));

    cout << "\nTop " << limit << " Most Popular Books:\n\n";
    cout << left
        << setw(COL_POP_RANK) << "Rank" << "| "
        << setw(COL_POP_ID) << "ID" << "| "
        << setw(COL_POP_TITLE) << "Title" << "| "
        << setw(COL_POP_AUTHOR) << "Author" << "| "
        << setw(COL_POP_BORROWED) << "Borrowed" << "| "
        << setw(COL_POP_TOTAL) << "Total" << "\n";
    int lineWidth = COL_POP_RANK + COL_POP_ID + COL_POP_TITLE + COL_POP_AUTHOR + COL_POP_BORROWED + COL_POP_TOTAL + (5 * 2); // 5 "| " separators
    cout << string(lineWidth, '-') << "\n";

    for (int i = 0; i < limit; i++) {
        const Book& book = sortedBooks[i];
        int borrowed = book.totalCopies - book.copiesAvailable;
        string title = book.title;
        if (static_cast<int>(title.length()) > COL_POP_TITLE - 1) {
            title = title.substr(0, COL_POP_TITLE - 4) + "...";
        }
        cout << left
            << setw(COL_POP_RANK) << (i + 1) << "| "
            << setw(COL_POP_ID) << book.bookID << "| "
            << setw(COL_POP_TITLE) << title << "| "
            << setw(COL_POP_AUTHOR) << book.author << "| "
            << setw(COL_POP_BORROWED) << borrowed << "| "
            << setw(COL_POP_TOTAL) << book.totalCopies << "\n";
    }
}

// Col width for fine summary report table
const int COL_FS_MEM_ID = 6;
const int COL_FS_NAME = 20;
const int COL_FS_RECORDED = 12;
const int COL_FS_ESTIMATED = 12;
const int COL_FS_TOTAL = 12;

void fineSummaryReport(const vector<Member>& members, int currentDay) {
    cout << "\n===== Fine Summary Report =====\n";

    cout << left
        << setw(COL_FS_MEM_ID) << "MemID" << "| "
        << setw(COL_FS_NAME) << "Name" << "| "
        << setw(COL_FS_RECORDED) << "Recorded" << "| "
        << setw(COL_FS_ESTIMATED) << "Estimated" << "| "
        << setw(COL_FS_TOTAL) << "Total" << "\n";
    int lineWidth = COL_FS_MEM_ID + COL_FS_NAME + COL_FS_RECORDED + COL_FS_ESTIMATED + COL_FS_TOTAL + (4 * 2); // 4 "| " separators
    cout << string(lineWidth, '-') << "\n";

    double totalFine = 0.00;
    bool found = false;
    for (const Member& member : members) {
        double estimatedFine = calculateEstimatedFine(member, currentDay);
        double memberTotal = member.fineBalance + estimatedFine;
        if (memberTotal > 0) {
            found = true;
            string name = member.name;
            if (static_cast<int>(name.length()) > COL_FS_NAME - 1) {
                name = name.substr(0, COL_FS_NAME - 4) + "...";
            }
            cout << left
                << setw(COL_FS_MEM_ID) << member.memberID << "| "
                << setw(COL_FS_NAME) << name << "| "
                << setw(COL_FS_RECORDED) << ("RM " + formatMoney(member.fineBalance)) << "| "
                << setw(COL_FS_ESTIMATED) << ("RM " + formatMoney(estimatedFine)) << "| "
                << setw(COL_FS_TOTAL) << ("RM " + formatMoney(memberTotal)) << "\n";
            totalFine += memberTotal;
        }
    }
    if (!found) cout << "No members have outstanding fines.\n";
    cout << "\n--------------------------------\nTotal Outstanding Fine (recorded + estimated): RM " << fixed << setprecision(2) << totalFine << "\n";
}

// ---------- File Export Functions ----------
string sanitizeFilename(const string& rawName) {
    string name;
    for (char c : rawName) {
        if (c == '/' || c == '\\' || c == ':') continue;
        name += c;
    }
    size_t firstNonDot = name.find_first_not_of('.');
    if (firstNonDot == string::npos) return "export.txt"; // was empty or all dots
    name = name.substr(firstNonDot);
    if (name.empty()) return "export.txt";
    return name;
}

// created automatically on first use , export with a folder and txt file as data record
string buildExportPath(const string& userFileName) {
    fs::path exportDir = fs::current_path() / "Smart Library System Data Export";
    error_code ec;
    fs::create_directories(exportDir, ec);
    return (exportDir / sanitizeFilename(userFileName)).string();
}

// Asks before silently overwriting a file that already exists.
// Returns true if it's OK to proceed with writing.
bool confirmOverwriteIfExists(const string& path) {
    if (!fs::exists(path)) return true;
    cout << "A file already exists at:\n" << path << "\nOverwrite it? (Y/N): ";
    string answer;
    getline(cin, answer);
    return !answer.empty() && (answer[0] == 'Y' || answer[0] == 'y');
}

/* Export book catalogue to a text file with a user-defined filename, saved into
   an exports folder created next to the program. */
void exportBooksToFile(const vector<Book>& books) {
    string userFileName = getValidString("Enter filename (e.g., books_report.txt): ");
    string fullPath = buildExportPath(userFileName);
    if (!confirmOverwriteIfExists(fullPath)) { cout << "Export cancelled.\n"; return; }
    ofstream outFile(fullPath);

    if (!outFile.is_open()) {
        cout << "Error: Could not open file for writing at path: " << fullPath << "\n";
        cout << "Tip: Please ensure you have write permissions in this folder.\n";
        return;
    }

    outFile << string(40, '=') << endl;
    outFile << "         LIBRARY BOOK CATALOGUE         \n";
    outFile << string(40, '=') << endl;
    outFile << left
        << setw(COL_BOOK_ID) << "ID" << "| "
        << setw(COL_BOOK_TITLE) << "Title" << "| "
        << setw(COL_BOOK_AUTHOR) << "Author" << "| "
        << setw(COL_BOOK_CATEGORY) << "Category" << "| "
        << setw(COL_BOOK_TOTAL) << "Total" << "| "
        << setw(COL_BOOK_AVAILABLE) << "Available" << "\n";
    outFile << string(80, '-') << "\n";

    for (const auto& book : books) {
        outFile << left
            << setw(COL_BOOK_ID) << book.bookID << "| "
            << setw(COL_BOOK_TITLE) << book.title << "| "
            << setw(COL_BOOK_AUTHOR) << book.author << "| "
            << setw(COL_BOOK_CATEGORY) << book.category << "| "
            << setw(COL_BOOK_TOTAL) << book.totalCopies << "| "
            << setw(COL_BOOK_AVAILABLE) << book.copiesAvailable << "\n";
    }

    outFile.close();
    cout << "Success! Book catalogue exported to full path:\n" << fullPath << "\n";
}

/* Export member records and fines to a text file with a user-defined filename,
   saved into an exports folder created next to the program. */
void exportMembersToFile(const vector<Member>& members, int currentDay) {
    string userFileName = getValidString("Enter filename (e.g., members_report.txt): ");
    string fullPath = buildExportPath(userFileName);
    if (!confirmOverwriteIfExists(fullPath)) { cout << "Export cancelled.\n"; return; }
    ofstream outFile(fullPath);

    if (!outFile.is_open()) {
        cout << "Error: Could not open file for writing at path: " << fullPath << "\n";
        cout << "Tip: Please ensure you have write permissions in this folder.\n";
        return;
    }

    outFile << string(40, '=') << endl;
    outFile << "          MEMBER & FINE RECORD          \n";
    outFile << string(40, '=') << endl;
    outFile << "Current Simulated Day: " << currentDay << "\n\n";

    outFile << left
        << setw(COL_MEM_ID) << "ID" << "| "
        << setw(COL_MEM_NAME) << "Name" << "| "
        << setw(COL_MEM_CATEGORY) << "Category" << "| "
        << setw(COL_MEM_RECORDED) << "Recorded" << "| "
        << setw(COL_MEM_ESTIMATED) << "Estimated" << "| "
        << setw(COL_MEM_TOTAL) << "Total Fine" << "| "
        << setw(COL_MEM_BORROWED) << "Borrowed" << "\n";
    outFile << string(85, '-') << "\n";

    for (const auto& member : members) {
        double estimatedFine = calculateEstimatedFine(member, currentDay);
        outFile << left
            << setw(COL_MEM_ID) << member.memberID << "| "
            << setw(COL_MEM_NAME) << member.name << "| "
            << setw(COL_MEM_CATEGORY) << member.category << "| "
            << setw(COL_MEM_RECORDED) << ("RM " + formatMoney(member.fineBalance)) << "| "
            << setw(COL_MEM_ESTIMATED) << ("RM " + formatMoney(estimatedFine)) << "| "
            << setw(COL_MEM_TOTAL) << ("RM " + formatMoney(member.fineBalance + estimatedFine)) << "| "
            << setw(COL_MEM_BORROWED) << to_string(member.borrowedCount) + "/" + to_string(getMaxBorrowLimit(member.category)) << "\n";
    }

    outFile.close();
    cout << "Success! Member records exported to full path:\n" << fullPath << "\n";
}

/* Export overdue books report to a text file with a user-defined filename,
   saved into an exports folder created next to the program. */
void exportOverdueReportToFile(const vector<Member>& members, const vector<Book>& books, int currentDay) {
    string userFileName = getValidString("Enter filename (e.g., overdue_report.txt): ");
    string fullPath = buildExportPath(userFileName);
    if (!confirmOverwriteIfExists(fullPath)) { cout << "Export cancelled.\n"; return; }
    ofstream outFile(fullPath);

    if (!outFile.is_open()) {
        cout << "Error: Could not open file for writing at path: " << fullPath << "\n";
        cout << "Tip: Please ensure you have write permissions in this folder.\n";
        return;
    }

    outFile << string(40, '=') << endl;
    outFile << "          OVERDUE BOOKS REPORT          \n";
    outFile << string(40, '=') << endl;
    outFile << " Current Simulated Day: " << currentDay << "\n\n";

    bool found = false;
    for (const Member& member : members) {
        int loanPeriodDays = getLoanPeriodDays(member.category);
        for (int i = 0; i < member.borrowedCount; i++) {
            int bookID = member.borrowedBooks[i].bookID;
            int borrowDay = member.borrowedBooks[i].borrowDay;
            int overdueDays = (currentDay - borrowDay) - loanPeriodDays;
            if (overdueDays > 0) {
                found = true;
                int bookIndex = findBookIndex(books, bookID);
                string title = (bookIndex != -1) ? books[bookIndex].title : "(unknown book)";
                double fine = overdueDays * FINE_RATE_PER_DAY;
                if (fine > MAX_FINE) fine = MAX_FINE;

                outFile << "Member ID: " << member.memberID << " | Name: " << member.name
                    << " | Book: " << title << " | Overdue: " << overdueDays << " days | Fine: RM " << formatMoney(fine) << "\n";
            }
        }
    }
    if (!found) {
        outFile << "No overdue books found at this time.\n";
    }

    outFile.close();
    cout << "Success! Overdue report exported to full path:\n" << fullPath << "\n";
}


// ---------- Menus ----------
void catalogueMenu(vector<Member>& members, vector<Book>& books, vector<Reservation>& reservations,
    bool copyBorrowed[][MAX_COPIES_PER_BOOK], int& nextBookSlot, int currentDay) {
    int choice;
    do {
        clearScreen();
        cout << "\n" << string(40, '=') 
             << "\n   Member & Book Catalogue Management   " 
             << "\n" << string(40, '=') << endl;

        cout << "  1.  Add Member    \n" 
             << "  2.  View Members  \n" 
             << "  3.  Search Member \n" 
             << "  4.  Update Member \n" 
             << "  5.  Delete Member \n"
             << "  6.  Add Book      \n" 
             << "  7.  View Books    \n" 
             << "  8.  Search Book   \n" 
             << "  9.  Update Book   \n" 
             << " 10.  Delete Book   \n"
             << " 11.  View Book Copy Status  \n" 
             << endl 
             << "  0. Back to Main Menu" << endl;
        
        choice = getValidInt("\nEnter choice: ", 0, 11);
        
        switch (choice) {
            case 1:
                addMember(members);
                break;
            case 2:
                viewMembers(members, currentDay);
                break;
            case 3:
                searchMember(members, currentDay);
                break;
            case 4:
                updateMember(members);
                break;
            case 5:
                deleteMember(members, reservations);
                break;
            case 6:
                addBook(books, copyBorrowed, nextBookSlot);
                break;
            case 7:
                viewBooks(books);
                break;
            case 8:
                searchBook(books);
                break;
            case 9:
                updateBook(books, copyBorrowed);
                break;
            case 10:
                deleteBook(books, reservations);
                break;
            case 11:
                viewBookCopyStatus(books, copyBorrowed);
                break;
            case 0:
                cout << "Returning to Main Menu...\n";
                break;
        }
        if (choice != 0) pauseForUser();
    } while (choice != 0);
}

void reservationMenu(vector<Member>& members, vector<Book>& books, vector<Reservation>& reservations) {
    int choice;
    do {
        clearScreen();
        cout << "\n" << string(40, '=')
             << "\n         Reservation Management" 
             << "\n" << string(40, '=') << endl;

        cout << "  1.  Reserve Book\n" 
             << "  2.  View Reservations\n" 
             << "  3.  Cancel Reservation\n" 
             << endl 
             << "  0.  Back to Main Menu" << endl;
        
        choice = getValidInt("\nEnter choice: ", 0, 3);
        
        switch (choice) {
            case 1:
                reserveBook(members, books, reservations);
                break;
            case 2:
                viewReservations(members, books, reservations);
                break;
            case 3:
                cancelReservation(members, books, reservations);
                break;
            case 0:
                cout << "Returning to Main Menu...\n";
                break;
        }
        if (choice != 0) pauseForUser();
    } while (choice != 0);
}

void borrowingMenu(vector<Member>& members, vector<Book>& books, vector<Reservation>& reservations, int& currentDay,
    bool copyBorrowed[][MAX_COPIES_PER_BOOK]) {
    int choice;
    do {
        clearScreen();
        cout << "\n" << string(40,'=') 
             << "\n         Borrowing & Returning          " 
             << "\n" << string(40,'=');

        cout << "\n  Current Simulated Day: " << currentDay << "\n\n";
        
        cout << "  1.  Borrow Book\n" 
             << "  2.  Return Book\n" 
             << "  3.  View Member's Borrowed Books\n" 
             << "  4.  Advance Day\n" 
             << endl
             << "  0.  Back to Main Menu\n";
        
        choice = getValidInt("\nEnter choice: ", 0, 4);
        
        switch (choice) {
            case 1:
                borrowBook(members, books, reservations, currentDay, copyBorrowed);
                break;
            case 2:
                returnBook(members, books, reservations, currentDay, copyBorrowed);
                break;
            case 3:
                viewBorrowedBooks(members, books, currentDay);
                break;
            case 4:
                advanceDay(currentDay);
                break;
            case 0:
                cout << "Returning to Main Menu...\n";
                break;
            }
        if (choice != 0) pauseForUser();
    } while (choice != 0);
}

void reportingMenu(vector<Member>& members, const vector<Book>& books, int currentDay) {
    int choice;
    do {
        clearScreen();
        cout << "\n" << string(40, '=')
             << "\n      Fine Calculation & Reporting      " 
             << "\n" << string(40, '=') << endl;

        cout << "  1.  View / Pay Fine\n"
             << "  2.  Overdue Books Report\n"
             << "  3.  Book Popularity Report\n"
             << "  4.  Fine Summary Report\n"
             << "  5.  Export Book Catalogue to File (TXT)\n"  
             << "  6.  Export Member Records to File (TXT)\n"   
             << "  7.  Export Overdue Report to File (TXT)\n" 
             << endl 
             << "  0.  Back to Main Menu\n";
        
        choice = getValidInt("\nEnter choice: ", 0, 7);
        
        switch (choice) {
            case 1: viewPayFine(members, currentDay); break;
            case 2: overdueBooksReport(members, books, currentDay); break;
            case 3: bookPopularityReport(books); break;
            case 4: fineSummaryReport(members, currentDay); break;
            case 5: exportBooksToFile(books); break;                   
            case 6: exportMembersToFile(members, currentDay); break;  
            case 7: exportOverdueReportToFile(members, books, currentDay); break;
            case 0: cout << "Returning to Main Menu...\n"; break;
        }
        
        if (choice != 0) pauseForUser();
    } while (choice != 0);
}

void mainMenu(vector<Member>& members, vector<Book>& books, vector<Reservation>& reservations, int& currentDay,
    bool copyBorrowed[][MAX_COPIES_PER_BOOK], int& nextBookSlot) {
    int choice;
    do {
        clearScreen();
        cout << endl
             << string(40, '=') << endl
             << "       LIBRARY MANAGEMENT SYSTEM\n"
             << string(40, '=') << endl;

        cout << " Current Simulated Day: " << currentDay << "\n\n";
        
        cout << "  1.  Member & Book Catalogue Management\n" 
             << "  2.  Reservation Management\n" 
             << "  3.  Borrowing & Returning\n" 
             << "  4.  Fine Calculation & Reporting\n" 
             << endl
             << "  0.  Exit\n";

        choice = getValidInt("\nEnter choice: ", 0, 4);
        
        switch (choice) {
            case 1:
                catalogueMenu(members, books, reservations, copyBorrowed, nextBookSlot, currentDay);
                break;
            case 2:
                reservationMenu(members, books, reservations);
                break;
            case 3:
                borrowingMenu(members, books, reservations, currentDay, copyBorrowed);
                break;
            case 4:
                reportingMenu(members, books, currentDay);
                break;
            case 0:
                cout << "\nExiting...\n";
                break;
        }
    } while (choice != 0);
}

int main() {
    // members and reservations start empty; books is preloaded
    vector<Member> members;
    vector<Book> books;
    vector<Reservation> reservations;
    int currentDay = 0;

    // copyBorrowed[slot][i]: true if physical copy i of the book in that slot is on loan.
    // nextBookSlot hands out a permanent, never-reused row for every title ever added.
    static bool copyBorrowed[MAX_BOOKS][MAX_COPIES_PER_BOOK] = {};
    int nextBookSlot = 0;

    initializeBooks(books, copyBorrowed, nextBookSlot);
    initializeMembers(members);

    cout << "Library Management System Started.\n";
    mainMenu(members, books, reservations, currentDay, copyBorrowed, nextBookSlot);
    cout << "Goodbye!\n";
    return 0;
}
