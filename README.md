# Smart Library System

A robust, console-based **Smart Library Management System** written in modern C++, designed to streamline library operations, member tracking, book catalogue management, reservations, and fine calculations with multi-category rule enforcement.

## 🚀 Key Features

* **Member & Category Management:** Supports three distinct member tiers (**Student**, **Staff**, and **Public**) with tailored borrowing limits (up to 5, 8, and 3 books) and customized loan periods (14, 21, and 7 days).
* **Advanced Book Catalogue:** Tracks book inventory by title, author, category, and total vs. available copies. Groups catalogue views by category and sorts them by popularity.
* **Per-Copy Physical Tracking:** Implements a stable internal matrix (`copyBorrowed`) ensuring physical copy tracking (Copy 1, Copy 2, etc.) that safely persists even if titles are updated or deleted.
* **FIFO Reservation Queue:** Automatically places holds on unavailable books in a First-In, First-Out queue, notifying the next eligible member upon return.
* **Dynamic Overdue & Fine Calculations:** Automatically computes accrued fines (RM 0.50 per day up to a max cap of RM 20.00 per loan), tracking both recorded balances and estimated unposted fines.
* **Secure File Export:** Safely exports book catalogues, member records, and overdue reports into structured text files (`.txt`) inside a dedicated local export folder with built-in path sanitization and overwrite protection.
