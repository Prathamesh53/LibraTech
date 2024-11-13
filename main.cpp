#include <iostream>
#include <mysql.h>
#include <mysqld_error.h>
#include <windows.h>
#include <sstream>
#include <mutex>  // Include the mutex header
using namespace std;

const char* HOST = "localhost";
const char* USER = "root";
const char* PW = "abc123#";
const char* DB = "mydb";

// Mutex to protect shared resource (book borrowing)
std::mutex mtx;

// Base class for common functionality (Admin, Student)
class Person {
public:
    virtual void display() = 0; // Pure virtual function for polymorphism
};

class Student : public Person {
private:
    string Id;
public:
    Student() : Id("") {}

    void setId(string id) {
        Id = id;
    }

    string getId() {
        return Id;
    }

    void display() override {
        cout << "Student ID: " << Id << endl;
    }
};

class Library : public Person {
private:
    string Name;
    int Quantity;
public:
    Library() : Name(""), Quantity(0) {}

    void setName(string name) {
        Name = name;
    }

    void setQuantity(int quantity) {
        Quantity = quantity;
    }

    int getQuantity() {
        return Quantity;
    }

    string getName() {
        return Name;
    }

    void display() override {
        cout << "Book Name: " << Name << ", Quantity: " << Quantity << endl;
    }
};

// Admin class to handle book and student additions
void admin(MYSQL* conn, Library &l, Student &s) {
    bool closed = false;
    while (!closed) {
        int choice;
        cout << "1. Add Book." << endl;
        cout << "2. Add Student." << endl;
        cout << "0. Exit." << endl;
        cout << "Enter Choice: ";
        cin >> choice;

        if (choice == 1) {
            system("cls");
            string name;
            int quantity;

            cout << "Book Name: ";
            cin >> name;
            l.setName(name);

            cout << "Enter Quantity: ";
            cin >> quantity;
            l.setQuantity(quantity);

            int Iq = l.getQuantity();
            stringstream ss;
            ss << Iq;
            string Sq = ss.str();

            string book = "INSERT INTO lib (Name,Quantity) VALUES('" + l.getName() + "', '" + Sq + "')";
            if (mysql_query(conn, book.c_str())) {
                cout << "Error: " << mysql_error(conn) << endl;
            } else {
                cout << "Book Inserted Successfully" << endl;
            }
        } else if (choice == 2) {
            system("cls");
            string id;
            cout << "Enter Student ID: ";
            cin >> id;
            s.setId(id);

            string st = "INSERT INTO student (Id) VALUES('" + s.getId() + "')";
            if (mysql_query(conn, st.c_str())) {
                cout << "Error: " << mysql_error(conn) << endl;
            } else {
                cout << "Student Inserted Successfully" << endl;
            }
        } else if (choice == 0) {
            closed = true;
            cout << "System is closing" << endl;
        }
    }
    Sleep(3000);
}

// Display function for available books
void display(MYSQL* conn) {
    system("cls");
    cout << "Available Books" << endl;
    cout << "***************" << endl;

    string disp = "SELECT * FROM lib";
    if (mysql_query(conn, disp.c_str())) {
        cout << "Error: " << mysql_error(conn) << endl;
    } else {
        MYSQL_RES* res;
        res = mysql_store_result(conn);
        if (res) {
            int num = mysql_num_fields(res);
            MYSQL_ROW row;
            while (row = mysql_fetch_row(res)) {
                for (int i = 0; i < num; i++) {
                    cout << " " << row[i];
                }
                cout << endl;
            }
            mysql_free_result(res);
        }
    }
}

// Function for borrowing a book with concurrent access handling using mutex lock
int borrowBook(MYSQL* conn) {
    system("cls");
    display(conn);

    string Sid;
    cout << "Enter Your ID: ";
    cin >> Sid;

    // SQL query to check if the student exists
    string com = "SELECT * FROM student WHERE Id = '" + Sid + "'";
    if (mysql_query(conn, com.c_str())) {
        cout << "Error: " << mysql_error(conn) << endl;
    } else {
        MYSQL_RES* res;
        res = mysql_store_result(conn);
        if (res) {
            int num = mysql_num_rows(res);
            if (num == 1) {
                cout << "Student ID Found." << endl;
                string Bname;
                cout << "Enter Book Name: ";
                cin >> Bname;

                // Lock the mutex to ensure only one thread can borrow a book at a time
                mtx.lock();  // Critical section starts

                // Start a transaction
                mysql_query(conn, "START TRANSACTION");

                // Lock the book record for update (pessimistic locking)
                string lockQuery = "SELECT Quantity FROM lib WHERE Name = '" + Bname + "' FOR UPDATE";
                if (mysql_query(conn, lockQuery.c_str())) {
                    cout << "Error: " << mysql_error(conn) << endl;
                    mysql_query(conn, "ROLLBACK");
                    mtx.unlock();  // Unlock the mutex after the operation
                    return 0;
                }

                MYSQL_RES* bookRes = mysql_store_result(conn);
                MYSQL_ROW bookRow = mysql_fetch_row(bookRes);

                if (bookRow) {
                    int quantity = atoi(bookRow[0]);

                    if (quantity > 0) {
                        // Decrement quantity atomically
                        int newQuantity = quantity - 1;
                        stringstream ss;
                        ss << newQuantity;
                        string updateQuery = "UPDATE lib SET Quantity = '" + ss.str() + "' WHERE Name = '" + Bname + "'";

                        if (mysql_query(conn, updateQuery.c_str())) {
                            cout << "Error: " << mysql_error(conn) << endl;
                            mysql_query(conn, "ROLLBACK");
                            mtx.unlock();  // Unlock the mutex after the operation
                            return 0;
                        } else {
                            cout << "Book is available. Borrowing Book...." << endl;
                        }

                        // Commit the transaction
                        mysql_query(conn, "COMMIT");
                    } else {
                        cout << "Book is not Available." << endl;
                        mysql_query(conn, "ROLLBACK");
                    }
                } else {
                    cout << "Book Not Found." << endl;
                    mysql_query(conn, "ROLLBACK");
                }

                mysql_free_result(bookRes);

                mtx.unlock();  // Critical section ends, release the lock
            } else if (num == 0) {
                cout << "This Student is Not Registered." << endl;
            }
        }
        mysql_free_result(res);
    }
    return 1;
}

// Main function
int main() {
    Student s;
    Library l;

    MYSQL* conn;
    conn = mysql_init(NULL);

    if (!mysql_real_connect(conn, HOST, USER, PW, DB, 3306, NULL, 0)) {
        cout << "Error: " << mysql_error(conn) << endl;
    } else {
        cout << "Logged In!" << endl;
    }
    Sleep(3000);

    bool exit = false;
    while (!exit) {
        system("cls");
        int val;
        cout << "Welcome To Library Management System" << endl;
        cout << "************************************" << endl;
        cout << "1. Administration." << endl;
        cout << "2. User." << endl;
        cout << "0. Exit." << endl;
        cout << "Enter Choice: ";
        cin >> val;

        if (val == 1) {
            system("cls");
            admin(conn, l, s);
        } else if (val == 2) {
            system("cls");
            borrowBook(conn);
        } else if (val == 0) {
            exit = true;
            cout << "Exiting...." << endl;
        }
    }

    mysql_close(conn);
    return 0;
}
