# Professional-Project: TFTP Server (RFC 1350, 2347, & 2348) 🚀

A robust, standards-compliant Trivial File Transfer Protocol (TFTP) server implementation written in **C**, designed for embedded networking and real-time operating environments (such as VxWorks).

---

## 📂 Repository Structure

| File Name | Description |
| :--- | :--- |
| **[`tftpd.c`](https://github.com/SazidAhmedTonmoy/Professional-Project/blob/main/tftpd.c)** | Core TFTP server logic, request handling, and packet parsing. |
| **[`tftpd.h`](https://github.com/SazidAhmedTonmoy/Professional-Project/blob/main/tftpd.h)** | Header definitions, protocol constants, opcodes, and structures. |
| **[`tftpd_cmd.c`](https://github.com/SazidAhmedTonmoy/Professional-Project/blob/main/tftpd_cmd.c)** | Command-line interface and control utilities for server management. |
| **[`tftpd_task.c`](https://github.com/SazidAhmedTonmoy/Professional-Project/blob/main/tftpd_task.c)** | Task management, background execution loops, and network routines. |

---

## ⚙️ Features & Compliance

* **RFC Compliance:** Implements core TFTP protocol functionality as specified in:
  * **[RFC 1350](https://datatracker.ietf.org/doc/html/rfc1350):** The TFTP Protocol (Revision 2).
  * **[RFC 2347](https://datatracker.ietf.org/doc/html/rfc2347):** TFTP Option Extension.
  * **[RFC 2348](https://datatracker.ietf.org/doc/html/rfc2348):** TFTP Blocksize Option.
* **Embedded Architecture:** Engineered for low-overhead execution, suitable for integration into network switches and embedded operating systems.
* **Modular Design:** Clean separation of protocol logic (`tftpd.c`), task handling (`tftpd_task.c`), and operator commands (`tftpd_cmd.c`).

---

## 🛠️ Tech Stack

* **Language:** C (100%)
* **Target Environments:** Embedded Linux, Wind River VxWorks RTOS

---

## 👤 Author

* **[Md Sazid Ahmed Tonmoy](https://github.com/SazidAhmedTonmoy)**
* [LinkedIn Profile](https://www.linkedin.com/in/sazidahmedtonmoy)
