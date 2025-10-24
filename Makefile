CFLAGS = -Wall -g -O2  -std=c++11
#
CC = c++
#CC = clang++ 
# -fno-inline  -DFREEBSD_ -DLINUX_

OBJSDIR = objs
#$(shell mkdir -p $(OBJSDIR))

DEPS = main.h string__.h classes.h bytes_array.h

OBJS = $(OBJSDIR)/yaik.o \
	$(OBJSDIR)/huffman_code.o \
	$(OBJSDIR)/http1_cgi.o \
	$(OBJSDIR)/http1_fcgi.o \
	$(OBJSDIR)/http2_cgi.o \
	$(OBJSDIR)/scgi.o \
	$(OBJSDIR)/http2_fcgi.o \
	$(OBJSDIR)/http1.o \
	$(OBJSDIR)/http2.o \
	$(OBJSDIR)/event_handler.o \
	$(OBJSDIR)/config.o \
	$(OBJSDIR)/functions.o \
	$(OBJSDIR)/classes.o \
	$(OBJSDIR)/ssl.o \
	$(OBJSDIR)/accept_connect.o \
	$(OBJSDIR)/socket.o \
	$(OBJSDIR)/percent_coding.o \
	$(OBJSDIR)/index.o \
	$(OBJSDIR)/log.o \

yaik: $(OBJS)
	$(CC) $(CFLAGS) -o $@  $(OBJS) -lpthread -L/usr/local/lib/ -L/usr/local/lib64/ -lssl -lcrypto

$(OBJSDIR)/yaik.o: yaik.cpp $(DEPS)
	$(CC) $(CFLAGS) -c yaik.cpp -o $@

$(OBJSDIR)/huffman_code.o: huffman_code.cpp $(DEPS)
	$(CC) $(CFLAGS) -c huffman_code.cpp -o $@

$(OBJSDIR)/http1_fcgi.o: http1_fcgi.cpp $(DEPS)
	$(CC) $(CFLAGS) -c http1_fcgi.cpp -o $@

$(OBJSDIR)/http2_fcgi.o: http2_fcgi.cpp $(DEPS)
	$(CC) $(CFLAGS) -c http2_fcgi.cpp -o $@

$(OBJSDIR)/ssl.o: ssl.cpp $(DEPS)
	$(CC) $(CFLAGS) -c ssl.cpp -o $@

$(OBJSDIR)/scgi.o: scgi.cpp $(DEPS)
	$(CC) $(CFLAGS) -c scgi.cpp -o $@

$(OBJSDIR)/http2_cgi.o: http2_cgi.cpp $(DEPS)
	$(CC) $(CFLAGS) -c http2_cgi.cpp -o $@

$(OBJSDIR)/http1_cgi.o: http1_cgi.cpp $(DEPS)
	$(CC) $(CFLAGS) -c http1_cgi.cpp -o $@

$(OBJSDIR)/config.o: config.cpp  $(DEPS)
	$(CC) $(CFLAGS) -c config.cpp -o $@

$(OBJSDIR)/http1.o: http1.cpp $(DEPS)
	$(CC) $(CFLAGS) -c http1.cpp -o $@

$(OBJSDIR)/http2.o: http2.cpp $(DEPS)
	$(CC) $(CFLAGS) -c http2.cpp -o $@

$(OBJSDIR)/classes.o: classes.cpp $(DEPS)
	$(CC) $(CFLAGS) -c classes.cpp -o $@

$(OBJSDIR)/accept_connect.o: accept_connect.cpp $(DEPS) 
	$(CC) $(CFLAGS) -c accept_connect.cpp -o $@

$(OBJSDIR)/event_handler.o: event_handler.cpp $(DEPS)
	$(CC) $(CFLAGS) -c event_handler.cpp -o $@

$(OBJSDIR)/socket.o: socket.cpp $(DEPS)
	$(CC) $(CFLAGS) -c socket.cpp -o $@

$(OBJSDIR)/percent_coding.o: percent_coding.cpp $(DEPS)
	$(CC) $(CFLAGS) -c percent_coding.cpp -o $@

$(OBJSDIR)/functions.o: functions.cpp $(DEPS)
	$(CC) $(CFLAGS) -c functions.cpp -o $@

$(OBJSDIR)/index.o: index.cpp $(DEPS)
	$(CC) $(CFLAGS) -c index.cpp -o $@

$(OBJSDIR)/log.o: log.cpp  $(DEPS)
	$(CC) $(CFLAGS) -c log.cpp -o $@

clean:
	rm -f yaik
	rm -f $(OBJSDIR)/*.o
