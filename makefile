CC = g++
PC = protoc
CFLAGS += -std=c++11
PROTOBUF_LIB := `pkg-config --cflags --libs protobuf`

message: Msg.proto
		$(PC) -I=. --cpp_out=. ./Msg.proto

process: Process.cpp Msg.pb.cc
		$(CC) $(CFLAGS) $^ $(PROTOBUF_LIB) -o $@

network:  Network.cpp Msg.pb.cc
		$(CC) $(CFLAGS) $^ $(PROTOBUF_LIB) -o $@

clean:
		rm network process