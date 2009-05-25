/*
	Copyright (c) 2007 Plecno s.r.l. All Rights Reserved 
	info@plecno.com
	via Giovio 8, 20144 Milano, Italy

	Released under the terms of the GPLv3 or later

	Author: Oreste Notelli <oreste.notelli@plecno.com>	
*/

#ifndef _sniffer_formatter_h
#define _sniffer_formatter_h
#include <vector>
#include <map>
#include <ostream>
#include <iostream>
#include <nids2.h>
#include <boost/shared_ptr.hpp>
#include <boost/iostreams/categories.hpp> 
#include <boost/iostreams/operations.hpp> 
#include <boost/iostreams/concepts.hpp>
#include "utilities.h"

class ascii_filter :public boost::iostreams::output_filter 
{
public:
	typedef char                   char_type;
    typedef boost::iostreams::output_filter_tag  category;

    template<typename Sink>
	bool put(Sink& snk, char c) 
	{ 
            return boost::iostreams::put(snk, (std::isprint(c) || (c=='\n' || c=='\t'))? c: '.'); 
	}
};

class handler: public shared_obj<handler>
{
public:
	virtual void onOpening(tcp_stream* pstream, const timeval* t) = 0 ;
	virtual void onOpen(tcp_stream* pstream, const timeval* t) = 0 ;
	virtual void onRequest(tcp_stream* pstream, const timeval* t) = 0 ;
	virtual void onResponse(tcp_stream* pstream,const  timeval* t) = 0 ;
	virtual void append(std::basic_ostream<char>& out, const timeval* t) = 0;
	virtual void onClose(tcp_stream* pstream, const timeval* ,unsigned char* packet) = 0;
	virtual ~handler(){}
};

class basic_handler : public handler
{
	virtual void append(std::basic_ostream<char>& out, const timeval* t) {}
	virtual void onOpening(tcp_stream* pstream, const timeval* t){}
	virtual void onOpen(tcp_stream* pstream, const timeval* t){}
	virtual void onRequest(tcp_stream* pstream, const timeval* t){}
	virtual void onResponse(tcp_stream* pstream,const  timeval* t){}
	virtual void onClose(tcp_stream* pstream, const timeval* ,unsigned char* packet){}
};

typedef std::vector<handler::ptr> handlers;

class handler_factory :public shared_obj<handler_factory>
{
public:
	virtual handler::ptr create_handler() = 0;
	virtual ~handler_factory(){}
};

template <class handler_t>
class handler_factory_t :public handler_factory
{
public:
	virtual handler::ptr create_handler()
	{
		return handler::ptr(new handler_t());
	}
};

template <class arg_t, class handler_t>
class handler_factory_t_arg :public handler_factory
{
public:
	handler_factory_t_arg(arg_t arg):_arg(arg){}
	virtual handler::ptr create_handler()
	{
		return handler::ptr(new handler_t(_arg));
	}
	arg_t _arg;
};

typedef std::vector<handler_factory::ptr> handler_factories;

class parse_element :public shared_obj<parse_element>
{
public:
	virtual const char* parse(const char* cursor, const string& _keyword, handler_factories& factories) = 0;
	virtual ~parse_element(){}
};

class keyword_common : public parse_element
{
protected:
	virtual const char* parse(const char* cursor, const string& _keyword, handler_factories& factories);
	virtual void push_back(handler_factories& factories)= 0;
	virtual ~keyword_common(){}
};

class keyword_base : public keyword_common
{
protected:
	virtual handler_factory::ptr create_new_factory() = 0;
	virtual void push_back(handler_factories& factories){factories.push_back(create_new_factory());}
};

class break_keyword_base : public keyword_base
{
protected:
	virtual const char* parse(const char* cursor, const string& _keyword, handler_factories& factories);
};

class keyword_params_base : public keyword_common
{
protected:
	virtual handler_factory::ptr create_new_factory(const string& params) = 0;
	virtual void push_back(handler_factories& factories){}
	const char* parse_params(const char* cursor );
	virtual const char* parse(const char* cursor, const string& _keyword, handler_factories& factories);
private:
	static const char start, end;
};


template <class handler_factory_t>
class keyword: public keyword_base
{
public:
	virtual handler_factory::ptr create_new_factory() 
	{
		return handler_factory::ptr(new handler_factory_t());
	}	
};

template <class handler_factory_t>
class break_keyword: public break_keyword_base
{
public:
	virtual handler_factory::ptr create_new_factory() 
	{
		return handler_factory::ptr(new handler_factory_t());
	}	
};

template <class arg_t, class handler_factory_t>
class keyword_arg: public keyword_base
{
public:
	keyword_arg(arg_t arg):_arg(arg){}
	virtual handler_factory::ptr create_new_factory() 
	{
		return handler_factory::ptr(new handler_factory_t(_arg));
	}	
private:
	arg_t _arg;
};

template <class arg_t, class arg2_t, class handler_factory_t>
class keyword_arg2: public keyword_base
{
public:
	keyword_arg2(arg_t arg, arg2_t arg2):_arg(arg),_arg2(arg2){}
	virtual handler_factory::ptr create_new_factory() 
	{
		return handler_factory::ptr(new handler_factory_t(_arg, _arg2));
	}	
private:
	arg_t _arg;
	arg2_t _arg2;
};

template <class handler_factory_t>
class keyword_params: public keyword_params_base
{
public:
	virtual handler_factory::ptr create_new_factory(const string& params) 
	{
		return handler_factory::ptr(new handler_factory_t(params));
	}	
};

class keyword_optional_params_base: public keyword_params_base
{
public:
	keyword_optional_params_base(const std::string& default_arg): _default_arg(default_arg){}
	virtual const char* parse(const char* cursor, const string& _keyword, handler_factories& factories);
private:
	string _default_arg;
};

template <class handler_factory_t>
class keyword_optional_params: public keyword_optional_params_base
{
public:
	keyword_optional_params(const string& default_arg): keyword_optional_params_base(default_arg){}
	virtual handler_factory::ptr create_new_factory(const string& params) 
	{
		return handler_factory::ptr(new handler_factory_t(params));
	}	
};

class printer : public shared_obj<printer>
{
public:
	virtual void doit(handlers::iterator start, handlers::iterator end, const timeval*t) = 0;
	virtual ~printer(){};
};

class outstream_printer : public printer
{
public:
	typedef std::basic_ostream<char>& Out;
	outstream_printer(Out out): _out(out){_out.setf(ios_base::fixed);}
	void doit(handlers::iterator start, handlers::iterator end,const timeval*t);
private :
	Out _out;
};

class cmd_execute_printer : public printer
{
public:
	typedef std::basic_ostream<char>& Out;
	cmd_execute_printer(std::string command): _command(command){}
	cmd_execute_printer(std::string command, std::string user): _command(command), _user(user){}
	void doit(handlers::iterator start, handlers::iterator end,const timeval*t);
private:
	void _execute(handlers::iterator start, handlers::iterator end,const timeval*t);
	std::string _command, _user;
};


class stream : public shared_obj<stream>
{
enum status_enum{unknown, opening, open, request, response, close};
public:
	stream(handler_factories::iterator _begin, handler_factories::iterator _end, printer* printer):
		begin(_begin), end(_end), status(unknown), _printer(printer){};
	virtual void onOpening(tcp_stream* pstream, const timeval* t);
	virtual void onOpen(tcp_stream* pstream, const timeval* t);
	virtual void onClose(tcp_stream* pstream, const timeval* t,unsigned char* packet);
	virtual void onRequest(tcp_stream* pstream, const timeval* t);
	virtual void onResponse(tcp_stream* pstream, const timeval* t);
	virtual ~stream(){};
	virtual void init();
	virtual void print(const timeval* t);

private:
	handler_factories::iterator begin, end;
	handlers _handlers;
	status_enum status;
	printer* _printer;
};

typedef std::basic_ostream<char>& Out;
// typedef std::basic_ostream<char>* pOut;

class parser
{
public:
	typedef std::map< std::string , parse_element::ptr> parse_elements;
	typedef std::map<struct tuple4, stream::ptr > streams;

	parser()
	{
		check(theOnlyParser==NULL, common_exception("parser::parser(): I am not the only parser"));
		theOnlyParser=this;
		init_parse_elements();
	}
	
	parser(printer* printer):_printer(printer)
	{
		check(theOnlyParser==NULL, common_exception("parser::parser(): I am not the only parser"));
		theOnlyParser=this;
		init_parse_elements();
	}
	parse_elements::iterator keywords_begin() {return elements.begin();}
	parse_elements::iterator keywords_end() {return elements.end();}
	static void nids_handler(struct tcp_stream *ts, void **yoda, struct timeval* t, unsigned char* packet);
	void parse(const char* format);
	virtual ~parser(){theOnlyParser = NULL;};
	void set_printer(printer* printer){_printer=printer;}
private:
	const char* parse_element(const char* format);
	void init_parse_elements();
	void process_opening_connection(tcp_stream *ts, struct timeval* t, unsigned char* packet);
	void process_open_connection(tcp_stream *ts, struct timeval* t, unsigned char* packet);
	void process_server(tcp_stream *ts, struct timeval* t, unsigned char* packet);
	void process_client(tcp_stream *ts, struct timeval* t, unsigned char* packet);
	void process_close_connection(tcp_stream *ts, struct timeval* t, unsigned char* packet);
	static parser* theOnlyParser;
	parse_elements elements;
	streams connections;
	handler_factories factories;
	printer* _printer;
public:
	
	static const char _key_word_id;
};

class unknown_keyword : public common_exception
{
public:
	unknown_keyword(const string& str): common_exception(string("unknown keyword '").append(str).append("'")){}
};

class parser_not_initialized: public common_exception
{
public:
	parser_not_initialized(): common_exception("parser non initializated"){}
};

class cannot_execute_command: public common_exception
{
public:
	cannot_execute_command(const string& command): common_exception(string("cannot execute command: ").append(command)){}
};

///// handlers /////
template <class base> class request_header_collector : public base
{
public:
	request_header_collector(): complete(false){}
	virtual void onRequest(tcp_stream* pstream, const timeval* t)
	{
		if (! complete)
			complete = get_headers(pstream->server.data, pstream->server.data + pstream->server.count_new, text);

	}
private:
	bool complete;
protected:
	string text;
};

template <class base> class response_header_collector : public base
{
public:
	response_header_collector(): complete(false){}
	virtual void onResponse(tcp_stream* pstream, const timeval* t)
	{
		if (! complete)
			complete = get_headers(pstream->client.data, pstream->client.data + pstream->client.count_new, text);
	}
private:
	bool complete;
protected:
	string text;
};

template <class base> class request_collector : public base
{
public:
	virtual void onRequest(tcp_stream* pstream, const timeval* t)
	{
		text += string(pstream->server.data, pstream->server.data + pstream->server.count_new);
	}
protected:
	string text;
};

template <class base> class response_collector : public base
{
public:
	virtual void onResponse(tcp_stream* pstream, const timeval* t)
	{
		text+= string (pstream->client.data, pstream->client.data + pstream->client.count_new);
	}
protected:
	string text;
};

class string_handler : public basic_handler
{
public:
	string_handler(const string& str):_str(str){};
	virtual void append(std::basic_ostream<char>& out, const timeval* ) {out << _str;};
private:
	std::string _str;
};

class string_handler_factory: public handler_factory 
{
public:
	string_handler_factory(const string & str):_str(str){}
	virtual handler::ptr create_handler() 
	{
		return handler::ptr(new string_handler(_str));
	}
private:
	std::string _str;
};

class connection_handler : public basic_handler
{

public:
	enum status {unknown, start,cont,last, uniq} stat;
	connection_handler():stat(unknown){};
	virtual void append(std::basic_ostream<char>& out, const timeval*  )
	{
		switch (stat)
		{
			case start:
				out << "start";
			break;
			case cont:
				out << "continue";
			break;
			case last:
				out << "last";
			break;
			case uniq:
				out << "unique";
			break;
			default:
				out << "unknown";
			break;
		}
	}
	
	virtual void onOpening(tcp_stream* pstream, const timeval* t){stat = start;}
	virtual void onOpen(tcp_stream* pstream, const timeval* t){stat = start;}
	virtual void onRequest(tcp_stream* pstream, const timeval* t){if (stat == unknown) stat = cont;}
	virtual void onResponse(tcp_stream* pstream,const  timeval* t){if (stat == unknown) stat = cont;}
	virtual void onClose(tcp_stream* pstream, const timeval* , unsigned char* packet){if (stat == start) stat = uniq; else stat = last;}
protected:
	u_long ip;
};

class ip_base : public basic_handler
{
public:
	ip_base():ip(0){}
	virtual void append(std::basic_ostream<char>& out, const timeval* ) {out <<ip_to_str(ip);};
protected:
	u_long ip;
};

class port_base : public basic_handler
{
public:
	port_base():port(0){}
	virtual void append(std::basic_ostream<char>& out, const timeval* ) {out <<int(port);};
protected:
	u_short port;
};


class source_ip : public ip_base
{
public:
	source_ip(){}
	virtual void onRequest(tcp_stream* pstream, const timeval* t)
	{
		ip = pstream->addr.saddr;
	}
};

class dest_ip : public ip_base
{
public:
	 dest_ip(){}
	virtual void onResponse(tcp_stream* pstream,const  timeval* t)
	{
		ip = pstream->addr.daddr;
	}
};

class source_port : public port_base
{
public:
	source_port(){}
	virtual void onRequest(tcp_stream* pstream, const timeval* t)
	{
		port = pstream->addr.source;
	}
};

class dest_port : public port_base
{
public:
	dest_port(){}
	virtual void onResponse(tcp_stream* pstream, const timeval* t)
	{
		port = pstream->addr.dest;
	}
};

class constant : public basic_handler
{
public:
	constant(const string& constant):_constant(constant){}
	virtual void append(std::basic_ostream<char>& out, const timeval* ) {out <<_constant;};
protected:
	string _constant;
};

template <class base>
class collect_first_line_request : public base
{
public:
	collect_first_line_request(): complete(false){}
	virtual void onRequest(tcp_stream* pstream, const timeval* t)
	{
		if (! complete)
			complete = get_first_line(pstream->server.data, pstream->server.data + pstream->server.count_new, text);

	}
private:
	bool complete;
protected:
	string text;
};

template <class base>
class collect_first_line_response : public base
{
public:
	collect_first_line_response(): complete(false){}
	virtual void onResponse(tcp_stream* pstream, const timeval* t)
	{
		if (! complete)
			complete = get_first_line(pstream->client.data, pstream->client.data + pstream->client.count_new, text);

	}
private:
	bool complete;
protected:
	string text;
};

class request_first_line: public collect_first_line_request<basic_handler>
{
	virtual void append(std::basic_ostream<char>& out,const timeval* ) {out <<text;}
};

class response_first_line: public collect_first_line_response<basic_handler>
{
	virtual void append(std::basic_ostream<char>& out,const timeval* ) {out <<text;}
};

class timestamp_handler : public basic_handler
{
public:
	timestamp_handler(const string& format):fmt(format){time.tv_sec = 0; time.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out,const timeval* ) {out <<timestamp(&time, fmt);};
protected:
	string fmt;
	timeval time;
};

class request_timestamp_handler : public timestamp_handler
{
public:
	request_timestamp_handler(const string& format):timestamp_handler(format){}
	virtual void onRequest(tcp_stream* pstream, const timeval* t){time=*t;}
};

class response_timestamp_handler : public timestamp_handler
{
public:
	response_timestamp_handler(const string& format):timestamp_handler(format){}
	virtual void onResponse(tcp_stream* pstream, const timeval* t){time=*t;}
};

class response_time_handler : public basic_handler
{
public:
	response_time_handler(){response = false;t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out,const timeval* ) {if (response) out <<to_double(t2-t1);else out<<0.0;}
	virtual void onOpen(tcp_stream* pstream, const timeval* t){t1=*t;}
	virtual void onResponse(tcp_stream* pstream, const timeval* t){response = true;t2=*t;}
	virtual void onRequest(tcp_stream* pstream, const timeval* t){t1=*t;}
	virtual void onClose(tcp_stream* pstream, const timeval* t, unsigned char* packet){if (!response) t2=*t;}

private:
	bool response;
	timeval t1, t2;
};

class request_time_handler : public basic_handler
{
public:
	request_time_handler(){requested_started = false;t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out,const timeval* ) {out <<to_double(t2-t1);}
	virtual void onRequest(tcp_stream* pstream, const timeval* t){if (!requested_started) t1=*t; t2=*t;requested_started= true;}

private:
	bool requested_started;
	timeval t1, t2;
};

class idle_time_2 : public basic_handler
{
public:
	idle_time_2(){response=false; t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out,const timeval* t) {t2=*t; if(response) out <<to_double(t2-t1);else out << int(0);}
	virtual void onResponse(tcp_stream* pstream, const timeval* t){t1=*t; response = true;}

private:
	timeval t1, t2;
	bool response;
};

class idle_time_1 : public basic_handler
{
public:
	idle_time_1(){open = false; request = false; t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void onOpen(tcp_stream* pstream, const timeval* t){t1=*t;open=true;}
	virtual void append(std::basic_ostream<char>& out,const timeval* t) {if (open && request) out <<to_double(t2-t1); else out << int(0);}
	virtual void onRequest(tcp_stream* pstream, const timeval* t){if (!request) t2=*t;request= true;}
private:
	timeval t1, t2;
	bool request, open;
};

class response_time_1 : public basic_handler
{
public:
	response_time_1(){response = false;t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out,const timeval* ) {if (response)out <<to_double(t2-t1); else out <<0.0;}
	virtual void onRequest(tcp_stream* pstream, const timeval* t){t1=*t;}
	virtual void onResponse(tcp_stream* pstream, const timeval* t){if (!response)t2=*t;response = true;}
private:
	timeval t1, t2;
	bool response;
};

class response_time_2 : public basic_handler
{
public:
	response_time_2(){response = false;t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out,const timeval* ) {if (response)out <<to_double(t2-t1);else out <<0.0;}
	virtual void onResponse(tcp_stream* pstream, const timeval* t){if (!response)t1=*t;t2=*t;response = true;}
private:
	bool response;
	timeval t1, t2;
};

class close_time : public basic_handler
{
public:
	close_time (){response = false; closed=false; t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out, const timeval* ) {if (response && closed) out <<to_double(t2-t1); else out << int(0);}
 	virtual void onOpen(tcp_stream* pstream, const timeval* t){response = true; t1=*t;}
 	virtual void onRequest(tcp_stream* pstream, const timeval* t){response = true; t1=*t;}
 	virtual void onResponse(tcp_stream* pstream, const timeval* t){response = true; t1=*t;}
	virtual void onClose(tcp_stream* pstream, const timeval* t, unsigned char* packet){t2=*t;closed=true;}
private:
	bool closed, response ;
	timeval t1, t2;
};

class close_originator : public basic_handler
{
public:
	close_originator (){closed=false, ip_originator=0, sip=0, dip=0;}
	virtual void append(std::basic_ostream<char>& out, const timeval* );
	virtual void onClose(tcp_stream* pstream, const timeval* t, unsigned char* packet);
	virtual void onRequest(tcp_stream* pstream, const timeval* t);
	virtual void onResponse(tcp_stream* pstream,const  timeval* t);
private:
	bool closed;
	u_int32_t ip_originator, sip, dip;
};

class connection_time_handler : public basic_handler
{
public:
	connection_time_handler(){connection_started = false;t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out, const timeval* ) {out <<to_double(t2-t1);}
	virtual void onOpening(tcp_stream* pstream, const timeval* t){if (!connection_started) t1=*t; ;connection_started= true;}
	virtual void onOpen(tcp_stream* pstream, const timeval* t){t2=*t;}

private:
	bool connection_started;
	timeval t1, t2;
};

/*
class session_time_handler : public basic_handler
{
public:
	session_time_handler(){t1.tv_sec = 0; t1.tv_usec= 0; t2.tv_sec = 0; t2.tv_usec= 0;}
	virtual void append(std::basic_ostream<char>& out, const timeval* ) {to_double(t2-t1);}
 	virtual void onOpen(tcp_stream* pstream, const timeval* t){ t2=*t;}
 	virtual void onRequest(tcp_stream* pstream, const timeval* t){t2=*t;}
 	virtual void onResponse(tcp_stream* pstream, const timeval* t){t2=*t;}
	virtual void onOpening(tcp_stream* pstream, const timeval* t){t1=*t; }
	virtual void onClose(tcp_stream* pstream, const timeval* t){t2=*t;}

private:
	timeval t1, t2;
};
*/

class response_size_handler : public basic_handler
{
public:
	response_size_handler():size(0){}
	virtual void append(std::basic_ostream<char>& out,const timeval* ) {out <<size;}
	virtual void onResponse(tcp_stream* pstream, const timeval* t)
	{
	  size+=pstream->client.count_new;
	}
private:
	int size;
};

class request_size_handler : public basic_handler
{
public:
	request_size_handler():size(0){}
	virtual void append(std::basic_ostream<char>& out, const timeval* ) {out <<size;}
	virtual void onRequest(tcp_stream* pstream, const timeval* t){size+=pstream->server.count_new;}
private:
	int size;
};


#endif// _sniffer_formatter_h