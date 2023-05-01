compile:
	sudo protoc --c_out=. new.proto
	gcc -o client3 client3.c new.pb-c.c -L/usr/local/lib -lprotobuf-c -lpthread	
	gcc -o server3 server3.c new.pb-c.c -L/usr/local/lib -lprotobuf-c -lpthread	
	