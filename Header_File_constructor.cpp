// Header_File_constructor.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>
#include <map>

std::regex include_reg("^#[ \t]*include[ \t]+\"([^\"]+)\"[ \t]*");
std::regex define_with_value_reg("^#[ \t]*define[ \t]+([a-zA-Z_][a-zA-Z0-9_]*)[ \t]+(.*)[ \t]*");
std::regex define_reg("^#[ \t]*define[ \t]+([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*");
std::regex undef_reg("^#[ \t]*undef[ \t]+([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*");
std::regex ifndef_reg("^#[ \t]*ifndef[ \t]+([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*");
//#if !defined(
std::regex ifndef_reg2("^#[ \t]*if[ \t]+!defined[ \t]*\\([ \t]*([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*\\)[ \t]*");
std::regex endif_reg("^#[ \t]*endif[ \t]*");

std::map<std::string, std::string> define_map;

class NullStream: public std::ostream {
	class NullBuffer: public std::streambuf {
	public:
		int overflow(int c) { return c; }
	} m_nb;

public:
	NullStream():
		std::ostream(&m_nb) {}
};
NullStream nullstream;

template<class char_t>
inline std::basic_string<char_t>& replace_all(std::basic_string<char_t>& a, const std::basic_string_view<char_t> b, const std::basic_string_view<char_t> c) {
	auto i = a.find(b);
	while(i != std::basic_string<char_t>::npos) {
		a.replace(i, b.size(), c);
		i = a.find(b, i + c.size());
	}
	return a;
};
template<class char_t, class T1, class T2>
inline std::basic_string<char_t>& replace_all(std::basic_string<char_t>& a, T1&& b, T2&& c) {
	return replace_all(a, std::basic_string_view<char_t>(b), std::basic_string_view<char_t>(c));
};


namespace arg_info {
	std::filesystem::path in_path;
	std::filesystem::path in_path_dir;
	std::filesystem::path out_path;
	bool				  is_full_mode = false;		  //-f or --full : Even under folder handling each file is guaranteed to be included individually without error
	bool				  open_help	   = false;		  //-h or --help : Display help
	std::string			  relocate_path;			  //-r or --relocate : Relocate the input file path in "#line" to the specified path
	bool				  relocate_path_was_an_url = false;
	bool				  format_line_beginning	   = false;		  //-f or --format : Format the line beginning
	bool				  using_std_out			   = false;		  //-s or --std-out : Output to standard output
}		// namespace arg_info

//路径转义为合法的C++字符串
inline std::string path_to_string(std::filesystem::path path) {
	std::string aret;
	if(!arg_info::relocate_path.empty()) {
		//get relative path of path to arg_info::in_path_dir
		auto relative_path = std::filesystem::relative(path, arg_info::in_path_dir);
		aret			   = arg_info::relocate_path + relative_path.string();
	}
	else
		aret = path.string();
	if(arg_info::relocate_path_was_an_url)
		replace_all(aret, "\\", "/");
	replace_all(aret, "//", "/");
	replace_all(aret, "\\\\", "\\");
	if(arg_info::relocate_path_was_an_url) {
		replace_all(aret, ":/", "://");
		replace_all(aret, " ", "%20");
	}
	return replace_all(aret, "\\", "\\\\");
}

void process_file(std::filesystem::path in_file, std::istream& in, std::ostream& out, std::string line_begin, std::filesystem::path include_path, std::filesystem::path root_path_for_skip) {
	std::string line;
	size_t		line_num = 0;
	//write #line
	out << line_begin << "#line 1 \"" << path_to_string(in_file) << "\"" << std::endl;
	//process file
	while(std::getline(in, line)) {
		line_num++;
		//get line begin of this line
		std::string line_begin_of_this_line;
		if(arg_info::format_line_beginning)
			line_begin_of_this_line = line_begin;
		{
			auto pos = line.find_first_not_of(" \t");
			if(pos != std::string::npos) {
				line_begin_of_this_line += line.substr(0, pos);
				line = line.substr(pos);
			}
		}
		std::smatch result;
		//match include
		if(std::regex_search(line, result, include_reg)) {
			std::string			  file_name		 = result[1];
			std::filesystem::path file_base_path = file_name;
			std::filesystem::path file_path		 = include_path / file_base_path;
			//get content after "#include"
			std::string content_after_include = line.substr(result.position() + result.length());
			//content_after_include must be empty or end with "//"
			if(content_after_include.empty() || content_after_include.find("//") == 0) {
				std::ifstream include_file(file_path);
				if(include_file.is_open()) {
					//if file_path's parent is root_path_for_skip, use NullStream
					//not skip the processing of the contents of the file as it may have definitions.
					if(!arg_info::is_full_mode && std::filesystem::equivalent(file_path.parent_path(), root_path_for_skip)) {
						out << line_begin_of_this_line << "#include \"" << file_path.filename().string() << "\"" << content_after_include << std::endl;
						process_file(file_path, include_file, nullstream, line_begin_of_this_line, file_path.parent_path(), root_path_for_skip);
					}
					else {
						if(!content_after_include.empty())		 //has comment
							out << line_begin_of_this_line << content_after_include << std::endl;
						process_file(file_path, include_file, out, line_begin_of_this_line, file_path.parent_path(), root_path_for_skip);
						//write #line to reset
						out << line_begin_of_this_line << "#line " << line_num << " \"" << path_to_string(in_file) << "\"" << std::endl;
					}
					include_file.close();
				}
				else {
					out << line_begin_of_this_line << line << std::endl;
					std::cerr << "can't open file: " << file_path << std::endl;
				}
			}
			else {
				out << line_begin_of_this_line << line << std::endl;
			}
		}
		//match define
		else if(std::regex_search(line, result, define_reg)) {
			std::string define_name = result[1];
			define_map[define_name] = "";
			out << line_begin_of_this_line << line << std::endl;
		}
		//match define_with_value
		else if(std::regex_search(line, result, define_with_value_reg)) {
			std::string define_name	 = result[1];
			std::string define_value = result[2];
			//get content after "#define"
			std::string content_after_define = line.substr(result.position() + result.length());
			//content_after_define must be empty or end with "//"
			if(content_after_define.empty() || content_after_define.find("//") == 0) {
				define_map[define_name] = define_value;
			}
			out << line_begin_of_this_line << line << std::endl;
		}
		//match undef
		else if(std::regex_search(line, result, undef_reg)) {
			std::string define_name = result[1];
			//get content after "#undef"
			std::string content_after_undef = line.substr(result.position() + result.length());
			//content_after_undef must be empty or end with "//"
			if(content_after_undef.empty() || content_after_undef.find("//") == 0) {
				define_map.erase(define_name);
			}
			out << line_begin_of_this_line << line << std::endl;
		}
		//match ifndef
		else if(std::regex_search(line, result, ifndef_reg)) {
		ifndef_reg_process:
			std::string define_name = result[1];
			//get content after "#ifndef"
			std::string content_after_ifndef = line.substr(result.position() + result.length());
			//content_after_ifndef must be empty or end with "//"
			if(content_after_ifndef.empty() || content_after_ifndef.find("//") == 0) {
				if(define_map.find(define_name) != define_map.end()) {
					//skip this line and all lines until #endif
					while(std::getline(in, line)) {
						line_num++;
						if(std::regex_search(line, result, endif_reg)) {
							//get content after "#endif"
							std::string content_after_endif = line.substr(result.position() + result.length());
							//content_after_endif must be empty or end with "//"
							if(content_after_endif.empty() || content_after_endif.find("//") == 0) {
								//write #line
								out << line_begin_of_this_line << "#line " << line_num << " \"" << path_to_string(in_file) << "\"" << std::endl;
								break;
							}
						}
					}
				}
				else {
					out << line_begin_of_this_line << line << std::endl;
				}
			}
			else {
				out << line_begin_of_this_line << line << std::endl;
			}
		}
		//match ifndef_reg2
		else if(std::regex_search(line, result, ifndef_reg2)) {
			goto ifndef_reg_process;
		}
		else {
			if(line.empty())
				out << std::endl;
			else
				out << line_begin_of_this_line << line << std::endl;
		}
	}
}

void process_file(std::string in_file_name, std::string out_file_name, std::filesystem::path root_path_for_skip) {
	std::cout << "process file: " << in_file_name << std::endl;
	std::ifstream		  in_file(in_file_name);
	std::ofstream out_file;
	std::ostream*		  out_stream = &out_file;
	if(arg_info::using_std_out)
		out_stream = &std::cout;
	else
		out_file.open(out_file_name, std::ios_base::binary);
	std::filesystem::path include_path = std::filesystem::path(in_file_name).parent_path();
	if(in_file.is_open() && (arg_info::using_std_out || out_file.is_open())) {
		process_file(in_file_name, in_file, out_file, "", include_path, root_path_for_skip);
		in_file.close();
		if(!arg_info::using_std_out)
			out_file.close();
	}
	else {
		std::cerr << "can't open file" << std::endl;
	}
	for(auto& [key, value]: define_map) {
		std::cout << "warning: define " << key << " is not undef" << std::endl;
	}
	define_map.clear();
	std::cout << "process file: " << in_file_name << " done\n\n"
			  << std::endl;
}


void print_help() {
	std::cout << "Usage: Header_File_constructor [options] in_file out_file" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -f, --full" << std::endl;
	std::cout << "    Even under folder handling each file is guaranteed to be included individually without error" << std::endl;
	std::cout << "  -h, --help" << std::endl;
	std::cout << "    Display help" << std::endl;
	std::cout << "  -r, --relocate" << std::endl;
	std::cout << "    Relocate the input file path in \"#line\" to the specified path" << std::endl;
	std::cout << "  -f, --format" << std::endl;
	std::cout << "    Format the line beginning" << std::endl;
	std::cout << "    This will result in a better looking output file," << std::endl;
	std::cout << "    but the number of columns in the compilation warning will not match the source file." << std::endl;
	std::cout << "  -s, --std-out" << std::endl;
	std::cout << "    Output to standard output" << std::endl;
	std::cout << "if in_file is a directory, out_file must be a directory or not exist," << std::endl;
	std::cout << "and all superficial files in in_file will be processed." << std::endl;
}

int main(size_t argc, char* _argv[]) {
	//build argv
	std::vector<std::string> argv;
	for(size_t i = 0; i < argc; i++) {
		argv.push_back(_argv[i]);
	}
	//process argv
	for(size_t i = 1; i < argv.size(); i++) {
		std::string arg = argv[i];
		if(arg == "-f" || arg == "--full") {
			arg_info::is_full_mode = true;
		}
		else if(arg == "-h" || arg == "--help") {
			arg_info::open_help = true;
		}
		else if(arg == "-f" || arg == "--format") {
			arg_info::format_line_beginning = true;
		}
		else if(arg == "-s" || arg == "--std-out") {
			arg_info::using_std_out = true;
		}
		else if(arg == "-r" || arg == "--relocate") {
			if(i + 1 < argv.size()) {
				arg_info::relocate_path = argv[i + 1];
				//if arg_info::relocate_path is an url, add a "/" at the end and replace all "\" with "/"
				if(arg_info::relocate_path.find("://") != std::string::npos) {
					arg_info::relocate_path_was_an_url = true;
					if(arg_info::relocate_path.back() != '/')
						arg_info::relocate_path += '/';
				}
				i++;
			}
			else {
				std::cerr << "error: -r or --relocate must be followed by a path" << std::endl;
				return 1;
			}
		}
		else {
			if(arg_info::in_path.empty()) {
				arg_info::in_path = arg;
			}
			else if(arg_info::out_path.empty()) {
				arg_info::out_path = arg;
			}
			else {
				std::cerr << "error: too many arguments" << std::endl;
				return 1;
			}
		}
	}
	if(arg_info::out_path.empty())
		arg_info::out_path = arg_info::in_path / ".out";
	if(arg_info::open_help || arg_info::in_path.empty()) {
		print_help();
		return 0;
	}
	arg_info::in_path  = std::filesystem::absolute(arg_info::in_path);
	arg_info::out_path = std::filesystem::absolute(arg_info::out_path);
	if(std::filesystem::is_regular_file(arg_info::in_path)) {
		arg_info::is_full_mode = true;
		if(!arg_info::using_std_out)
			std::filesystem::create_directories(arg_info::out_path.parent_path());
		arg_info::in_path_dir = arg_info::in_path.parent_path();
		process_file(arg_info::in_path.string(), arg_info::out_path.string(), arg_info::in_path_dir);
	}
	else if(std::filesystem::is_directory(arg_info::in_path)) {
		if(!arg_info::using_std_out)
			std::filesystem::create_directories(arg_info::out_path);
		arg_info::in_path_dir = arg_info::in_path;
		//process just superficial files
		for(auto& p: std::filesystem::directory_iterator(arg_info::in_path)) {
			if(std::filesystem::is_regular_file(p)) {
				std::filesystem::path in_file_path	= p;
				std::filesystem::path out_file_path = arg_info::out_path / in_file_path.filename();
				process_file(in_file_path.string(), out_file_path.string(), arg_info::in_path_dir);
			}
		}
	}
	else {
		std::cerr << "error: " << arg_info::in_path << " is not a file or directory" << std::endl;
		return 1;
	}
	return 0;
}
