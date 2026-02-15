# Server uses HTTP1.1 and HTTP2(h2) protocol

Written in C++ (version C++11) with using C functions.

Tested on OS: Debian, OpenBSD, FreeBSD

### Features:
 * Methods: GET, POST, HEAD
 * Partial content
 * Directory indexing
 * CGI
 * FastCGI
 * PHP-FPM
 * SCGI

### HTTP2: 
 * ALPN
 * Stream flow control
 * Dynamic Table of Header Fields

### Not supported:
 * Server push
 * Stream prioritization
 * Frames CONTINUATION
 * Cookies
 * Compress data
 * Caching
 * And everything that was not included in the Features

### Compiling and run:
 * Install libraries: OpenSSL or LibreSSL.  
 * cd yaik/  
 * mkdir objs/  
 * mkdir certs/  
 * make clean  
 * make  

 * Edit configuration file: yaik.conf  
 * ./yaik  
