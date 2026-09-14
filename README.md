# Smart Library System

A robust, console-based **Smart Library Management System** written in modern C++, designed to streamline library operations, member tracking, book catalogue management, reservations, fine calculations, and data exporting.

## Features

* **Member & Category Management:** Supports three distinct member tiers (**Student**, **Staff**, and **Public**) with tailored borrowing limits (up to 5, 8, and 3 books) and customized loan periods (14, 21, and 7 days).
* **Advanced Book Catalogue:** Tracks book inventory by title, author, category, and total vs. available copies. Groups catalogue views by category and sorts them by popularity.
* **Per-Copy Physical Tracking:** Implements a stable internal matrix (`copyBorrowed`) ensuring physical copy tracking (Copy 1, Copy 2, etc.) that safely persists even if titles are updated or deleted.
* **FIFO Reservation Queue:** Automatically places holds on unavailable books in a First-In, First-Out queue, notifying the next eligible member upon return.
* **Dynamic Overdue & Fine Calculations:** Automatically computes accrued fines (RM 0.50 per day up to a max cap of RM 20.00 per loan), tracking both recorded balances and estimated unposted fines.
* **Secure File Export:** Safely exports book catalogues, member records, and overdue reports into structured text files (`.txt`) inside a dedicated local export folder with built-in path sanitization and overwrite protection.

## System Menu

1. **Member & Book Catalogue Management**
   * Add, view, search, update, and delete members or books.
   * View detailed physical copy status of individual titles.
2. **Reservation Management**
   * Place book reservations, view the active FIFO queue, or cancel existing holds.
3. **Borrowing & Returning**
   * Process book checkouts and returns with fine checks and physical copy assignments.
   * Simulate the passage of time (`Advance Day`) for testing overdues.
4. **Fine Calculation & Reporting**
   * View and pay recorded fines.
   * Generate Overdue Books Report, Book Popularity Report, and Fine Summary Report.
   * Export reports directly to `.txt` files.

## Technical Stack

* **Language:** C++ (Modern C++ standards, utilizing standard template library like `vector`, `map`, `algorithm`)
* **Environment:** Cross-platform (Windows, macOS, Linux)
* **File Handling & System:** `<fstream>` for report generation and `<filesystem>` for safe, cross-platform export directory management.

## Screenshots
### Main Menu
* <img width="360" height="230" alt="image" src="https://github.com/user-attachments/assets/d0979a9a-aba0-4a3a-8bf9-1efc37d46844" />

### Module 1: Member & Book Catalogue Management
* <img width="360" height="290" alt="image" src="https://github.com/user-attachments/assets/bb502706-6087-4603-be79-c945f3334cc2" />

* **1. Add Member:** Register a new user by inputting a unique ID, name, and selecting a category (**Student**, **Staff**, or **Public**) to assign tailored borrowing limits and loan periods.
* <img width="560" height="230" alt="image" src="https://github.com/user-attachments/assets/5bddc4b0-ade4-41ae-9173-7c2e013cd351" />
* <img width="560" height="200" alt="image" src="https://github.com/user-attachments/assets/248888e8-c879-49e1-a653-d234cc39efb8" />
* <img width="560" height="230" alt="image" src="https://github.com/user-attachments/assets/98d1c4d4-e30b-413c-b96e-cb0df07af6d7" />

* **2. View Members:** Display a formatted table listing all registered members, their categories, recorded fines, estimated unposted fines, total fines, and current active loans.
* <img width="940" height="190" alt="image" src="https://github.com/user-attachments/assets/86374b5e-cc86-43e6-956f-cb383fb3f467" />

* **3. Search Member:** Look up a specific member by their Member ID to review their profile and account status.
* <img width="940" height="190" alt="image" src="https://github.com/user-attachments/assets/d71edcd6-efc4-410b-a0db-7699293de744" />
* <img width="940" height="190" alt="image" src="https://github.com/user-attachments/assets/795e3148-9dd0-4762-ae16-48480fff157f" />

* **4. Update Member:** Modify an existing member's name or membership category, complete with safety validation checks against active loans.
* <img width="940" height="250" alt="image" src="https://github.com/user-attachments/assets/b608476d-9f73-47cc-a0e6-a15435074f7f" />
* <img width="940" height="120" alt="image" src="https://github.com/user-attachments/assets/6350af21-f853-4708-9d3f-97a343aad7e5" />

* **5. Delete Member:** Remove a member from the system, conditioned on them having zero borrowed books and no pending reservation holds.
* <img width="935" height="120" alt="image" src="https://github.com/user-attachments/assets/5762d1ca-789a-41e1-86f9-2ca61f4dd582" />
* <img width="935" height="100" alt="image" src="https://github.com/user-attachments/assets/1168d179-bd75-411f-a716-6ee66005c226" />

* **6. Add Book:** Register a new book title with a unique ID, title, author, category, and total physical copy count (up to the maximum copy matrix limit).
* <img width="930" height="190" alt="image" src="https://github.com/user-attachments/assets/bd42a6c2-0e05-41d4-8917-ea8c2e1638d7" />

* **7. View Books:** Display the complete library catalogue neatly grouped by academic categories and sorted by volume/popularity.
* <img width="900" height="580" alt="image" src="https://github.com/user-attachments/assets/5e8358ac-3222-4e21-85e8-4a73f55de095" />

* **8. Search Book:** Instantly locate a specific book title using its Book ID.
* <img width="931" height="139" alt="image" src="https://github.com/user-attachments/assets/c959ceef-a91e-4684-a74b-b6e247ba7229" />

* **9. Update Book:** Edit metadata (title, author, category) or adjust the total physical copy count of an existing book.
* <img width="930" height="189" alt="image" src="https://github.com/user-attachments/assets/c94cbe0b-91b3-44c4-bc06-9baac06a7938" />
* <img width="920" height="52" alt="image" src="https://github.com/user-attachments/assets/13f2b5a5-b814-4205-9bb5-f96874016796" />

* **10. Delete Book:** Remove a book title from the system, provided all physical copies are currently on the shelf and no active reservations exist.
* <img width="929" height="90" alt="image" src="https://github.com/user-attachments/assets/712d667b-3f26-4da3-845e-bf0a6b79d64f" />
* <img width="923" height="126" alt="image" src="https://github.com/user-attachments/assets/0e230bac-e8ab-4c52-af1d-fb3e823e2bb6" />

* **11. View Book Copy Status:** Inspect the internal copy tracking matrix for a specific book to view individual physical copy states (`On Shelf` vs `BORROWED`).
* <img width="921" height="204" alt="image" src="https://github.com/user-attachments/assets/c20ee54b-8491-4cd0-8c2d-ccddca6ec6ec" />


### Module 2: Reservation Management
* <img width="936" height="193" alt="image" src="https://github.com/user-attachments/assets/e5faf094-fe34-43b4-90c5-668be527d9fe" />

* **1. Reserve Book:** Place a hold on a book whose physical copies are fully checked out (`copiesAvailable == 0`), automatically queuing the member into a FIFO order.
* <img width="927" height="152" alt="image" src="https://github.com/user-attachments/assets/d1b55739-dc56-4747-becf-065be1cd3b70" />

* **2. View Reservations:** List all active book holds across the library, showing queue positions, member IDs, names, book IDs, and titles.
* <img width="929" height="138" alt="image" src="https://github.com/user-attachments/assets/23dc897e-e24b-49e4-b1fc-55080101a0da" />

* **3. Cancel Reservation:** Remove an active hold from the reservation queue for a specific member and book.
* <img width="932" height="141" alt="image" src="https://github.com/user-attachments/assets/8aa26d85-d120-46cb-8ba9-fd0889d8b446" />
* <img width="925" height="50" alt="image" src="https://github.com/user-attachments/assets/8e04bfaf-87cf-484c-91d3-b8c885993240" />

### Module 3: Borrowing & Returning
* **1. Borrow Book:** Check out an available book to a member, verifying that they have zero outstanding fines and haven't exceeded their category limit, then assign a physical copy.
* <img width="926" height="750" alt="image" src="https://github.com/user-attachments/assets/aa86b8ec-ee8f-4b1e-b705-928f23d071b3" />

* **2. Return Book:** Process a return, automatically compute and assign overdue penalties (if past the category-specific loan period), restore inventory, and trigger FIFO queue notifications.
* <img width="929" height="191" alt="image" src="https://github.com/user-attachments/assets/13a65ab5-0cd9-4295-9679-b761dc2fe959" />

* **3. View Member's Borrowed Books:** Inspect an individual member's active loans, due days, overdue days, status (`OK` or `OVERDUE`), and estimated unposted fines.
* <img width="931" height="231" alt="image" src="https://github.com/user-attachments/assets/fb961a6a-b48f-4279-a16e-cfe147281164" />

* **4. Advance Day:** Simulate the passage of time by adding custom simulated days to test fine accumulation and overdue escalations.
* <img width="932" height="132" alt="image" src="https://github.com/user-attachments/assets/89f94157-69c5-441e-b40d-91aec246f4ba" />
* <img width="932" height="223" alt="image" src="https://github.com/user-attachments/assets/29d46b85-d109-423a-862d-c6b8b5c6b35e" />
* <img width="931" height="198" alt="image" src="https://github.com/user-attachments/assets/14032654-3ffd-4dcb-b587-9ed23bea0b7e" />

### Module 4: Fine Calculation & Reporting
* <img width="931" height="312" alt="image" src="https://github.com/user-attachments/assets/d1f768af-24de-4cbb-a56a-2687e9a104c5" />

* **1. View / Pay Fine:** Inspect a member's financial breakdown (recorded vs. estimated unposted fines) and process partial or full fine payments.
* <img width="1607" height="280" alt="image" src="https://github.com/user-attachments/assets/9ffe5162-17ad-4a21-95e7-f2c84e1cbda4" />

* **2. Overdue Books Report:** Generate a comprehensive list of all currently overdue books across all members along with calculated penalties.
* <img width="780" height="128" alt="image" src="https://github.com/user-attachments/assets/6b33ac73-ca36-4d4a-b4b0-7226e2046037" />

* **3. Book Popularity Report:** Display the top 5 most frequently borrowed books ranked by active loan counts.
* <img width="925" height="219" alt="image" src="https://github.com/user-attachments/assets/d9ff6d3c-f18c-4b10-9491-0a7a7e5852da" />

* **4. Fine Summary Report:** View an aggregated financial summary of all outstanding fines across the entire library system.
* <img width="692" height="264" alt="image" src="https://github.com/user-attachments/assets/0b097c0f-4f19-4238-9dea-e539ba6385df" />

* **5. Export Book Catalogue to File:** Safely export the entire book inventory into a structured `.txt` file within the local export directory.
* <img width="791" height="191" alt="image" src="https://github.com/user-attachments/assets/0c26a0a9-ed0e-4d98-9244-b79d943da207" />
* <img width="816" height="198" alt="image" src="https://github.com/user-attachments/assets/5cb3157c-00dd-4352-a84d-1762b02b564b" />

* **6. Export Member Records to File:** Export complete member profiles, fine balances, and borrowing quotas into a local text report.
* <img width="695" height="142" alt="image" src="https://github.com/user-attachments/assets/0abe12a5-2050-4e9a-b50c-6e53fbddd78a" />
* <img width="814" height="270" alt="image" src="https://github.com/user-attachments/assets/aee8769a-92e7-4e1b-9cfa-c2bd600fab46" />

* **7. Export Overdue Report to File:** Generate and save a dedicated text report of all current overdue loans with full path protection and overwrite verification.
* <img width="654" height="128" alt="image" src="https://github.com/user-attachments/assets/5f39dbd7-8ee0-48cb-81d7-1a9dd2e1c320" />
* <img width="802" height="308" alt="image" src="https://github.com/user-attachments/assets/285835b3-e9da-453e-b2fe-3d817afac5ae" />

## Installation & Running

1. Open the project source code (`Smart Library System trial.cpp`) in your preferred C++ IDE or text editor (e.g., Visual Studio, CLion, or VS Code).
2. Compile the code using a C++17 (or newer) compatible compiler:
   ```bash
   g++ -std=c++17 "Smart Library System trial.cpp" -o library_system
