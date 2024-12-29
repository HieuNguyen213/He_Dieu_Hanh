*************************************Hướng dẫn cài đặt và chạy chươn trình*************************************************************
**************************************************************************************************************************************
Link github: 
https://github.com/HieuNguyen213/He_Dieu_Hanh/tree/master

Video hướng dẫn cài đặt và chạy chương trình: https://www.youtube.com/watch?v=IwF95N5tz4U

Các bước cài đặt và chạy chương trình:
B1: Tải project từ link github.

B2: Chạy terminal ở chế độ wsl.
    Sau đó thực hiện cài OpenSSL và các thư viện cần thiết bằng các câu lệnh sau:
	sudo apt update
	sudo apt upgrade
	sudo apt install openssl
	sudo apt install libssl-dev


B3: Sau khi tải xong, biên dịch file server.c và client.c bằng hai câu lệnh sau:
	gcc server.c -o server -lssl -lcrypto  
	gcc client.c -o client -lssl -lcrypto

B4: Sau khi có file thực thi là server và client rồi thì tiến hành chạy lần lượt bằng hai câu lệnh sau:
	./server
	./client <địa chỉ IP của server>

	Sau khi chạy hai câu lệnh này thì một folder tên là storage2 sẽ được tạo ra. Thư mục này dùng để lưu trữ những thư mục gửi từ client đến server.

B5: Chạy các command, tuỳ theo mục đích sử dụng:
	ls: 							Hiện cây thư mục trên server
rsync <thư mục nguồn> <thư mục đích>:		Đồng bộ từ client lên server
clone <thư mục nguồn> <thư mục đích>		Đồng bộ từ server về client




*************************************Ý tưởng đồng bộ từ client lên server*************************************************************
**************************************************************************************************************************************
2 trường hợp: đường dẫn đến folder để lưu trên server đã tồn tại
hoặc đường dẫn đến folder để lưu chưa tồn tại.

TH1: Đường dẫn đến folder để lưu trên server chưa tồn tại
- Tiến hành tài file và thư mục con (các file trong thư mục con) từ client
lên server như bình thường.

TH2: Đường dẫn đến folder để lưu trên server đã tồn tại.
- Sẽ tiến hành gửi thông tin từng file lên server (sẽ bao gồm tên file, đường dẫn (different
path), hash) lên server. Đầu tiên Server sẽ dùng cái file name nhận được để kiểm tra xem file đó trong
thư mục cần lưu trên server đã tồn tại chưa. Như vậy sẽ có 2 trường hợp:

	TH2.1: Chưa có file nào tên giống như vậy trong folder. Gửi response lại cho client để client
		gửi thông tin cái file đó đến server (tất nhiên là sẽ gồm cả tên kích thước, nội dung).
	TH2.2: Có file có tên giống như vậy trong folder. Như vậy sẽ tiến hành so sánh hash của file có
		trong folder trên server với hash nhận được từ client. Như vậy sẽ chia ra tiếp làm 2 trường hợp.

		TH2.2.1: Nếu hash không giống nhau thì gửi response lại cho client để client
			gửi thông tin cái file đó đến server (tất nhiên là sẽ gồm cả tên kích thước, nội dung).
		TH2.2.2: Nếu hash giống nhau thì thôi, kết thúc, chuyển sang file tiếp theo (nếu có).



Ý tưởng đồng bộ từ server về client: Về cơ bản thì chỉ là tải thư mục từ server về client
2 trường hợp: folder cần tải từ server về client đã tồn tại trên client (tên folder đã tồn tại) hoặc folder đó chưa tồn tại

TH1: Folder đó chưa tồn tại
- Tiến hành tải toàn bộ folder từ server về client

TH2: Folder đã tồn tại
- Lúc này tiến hành gửi thông tin từng file từ server về client. Khi nhận được thông tin file thì client tiến hành kiểm tra xem
có file nào tồn tại chưa (dựa vào file name).
	TH2.1: Không có file name nào trùng khớp. Như vậy tức là không có file đó trên client. Vậy thì chỉ việc tải toàn bộ file
	       đó từ server về.
	TH2.2: Tồn tại file name trùng khớp. Đây chính là bước cần thực hiện đồng bộ. Tiến hành so sánh hash của file trên client
		với hash nhận được từ server

		TH2.2.1: Nếu giống nhau (tức là file trên client với trên server không có gì thay đổi) thì không thực hiện gì, chuyển
		sang file tiếp theo.
		TH2.2.2: Nếu hash khác nhau, tức là file trên client với file trên server không còn giống nhau nữa, đã có sự thay đổi.
		Lúc này sẽ xét tiếp 1 tham số nữa gọi là timestamp, tức là thời gian chỉnh sửa của từng file. Nếu thời gian chỉnh sửa của
		file trên server là trước file trên client thì hiện ra thông báo để người dùng lựa chọn xem là có chọn đồng bộ theo file trên
		server hay không, còn nếu file trên server có thời gian chỉnh sửa là sau client thì không cần hiện thông báo.
		=> Đảm bảo có thể đồng bộ file dựa theo version mới nhất.




*************************************Câu lệnh để biên dịch chương trình***************************************************************
**************************************************************************************************************************************
CHÚ Ý: Phải biên dịch chương trình để tạo thư mục lưu trữ
- Biên dịch cho client: gcc client.c -o client -lssl -lcrypto
- Biên dịch cho server: gcc server.c -o server -lssl -lcrypto



*************************************Command*****************************************************************************************
**************************************************************************************************************************************
ls: 						Hiện cây thư mục trên server
rsync <thư mục nguồn> <thư mục đích>:		Đồng bộ từ client lên server
clone <thư mục nguồn> <thư mục đích>		Đồng bộ từ server về client








