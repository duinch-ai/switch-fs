# Active Plan — Switch FS (Nintendo Switch FreeShop)

## Objective
Xây dựng ứng dụng FreeShop tải và cài đặt game Nintendo Switch kết nối trực tiếp API `nswgf.com` (15.800+ tựa game). Chạy thử nghiệm và hoàn thiện giả lập trên macOS trước khi build lên Switch phần cứng.

## Task Tracking
- [x] [DONE] SFS-01: Cấu hình CMake Desktop macOS và build test cửa sổ Borealis giả lập
- [x] [DONE] SFS-02: Hoàn thiện `NswgfApi` client (lấy danh sách, phân trang, regex parse metadata)
- [x] [DONE] SFS-03: Xây dựng widget `GameCard` hiển thị bìa game và focus
- [x] [DONE] SFS-04: Xây dựng `HomeTab` và phân trang danh sách game (Load More pagination)
- [x] [DONE] SFS-05: Xây dựng `SearchTab` tìm kiếm game theo từ khóa (ImeManager virtual keyboard)
- [x] [DONE] SFS-06: Xây dựng `GameDetailView` chi tiết game và danh sách link host
- [x] [DONE] SFS-07: Xây dựng `DownloadManager` đa luồng libcurl với tiến độ & tốc độ
- [x] [DONE] SFS-08: Tích hợp Gofile / 1fichier host selector & direct link handling
- [x] [DONE] SFS-09: Xây dựng tab quản lý hàng đợi tải `DownloadQueueTab` (`QueueTab`)
- [x] [DONE] SFS-10: Tích hợp `libarchive` giải nén file `.rar` tự động sau tải
- [x] [DONE] SFS-11: Kiểm thử toàn diện quy trình qua bộ Unit Test Suite (17/17 tests PASS)
- [x] [DONE] SFS-12: Tích hợp động cơ cài đặt `NspInstaller` (`libnx` es/ncm/ns) trên Nintendo Switch
- [x] [DONE] SFS-13: Xây dựng `SettingsTab` & `SettingsManager` lưu trữ config JSON
- [x] [DONE] SFS-14: Đóng gói Forwarder NSP package (`scripts/forwarder`) cài icon ngoài Home Menu như Tinfoil
- [x] [DONE] SFS-15: Cấu hình GitHub Actions CI build release `SwitchFS.nro` tự động bằng devkitPro
