#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "reports_queue.h"

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
using socklen_type = int;
static void close_socket(socket_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
using socklen_type = socklen_t;
static void close_socket(socket_t s) { close(s); }
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

namespace fs = std::filesystem;
const int PORT = 8000;
const std::string DATA_DIR = "data";
const std::string PROFILE_DIR = "data/profiles";
const std::string REPORT_DIR = "data/reports";

std::mutex data_mutex;
ReportQueue incoming_reports;

std::mutex admin_mutex;
std::set<std::string> admin_tokens;

std::string get_admin_password() {
    const char* env = std::getenv("BAGSPHERE_ADMIN_PASSWORD");
    if (env && *env) return std::string(env);
    return "bagsphere123";
}
const std::string ADMIN_PASSWORD = get_admin_password();

struct BagProfile {
    std::string bag_id, student_name, student_id, phone, message, photo_data, owner_token;
};

std::string html_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'': o += "&#39;"; break;
            default: o += c;
        }
    }
    return o;
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else o += c;
    }
    return o;
}

std::string random_id(const std::string& prefix, int bytes = 4) {
    static std::mt19937_64 rng(
        (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const char* hex = "0123456789ABCDEF";
    std::string s = prefix;
    for (int i = 0; i < bytes * 2; ++i) s += hex[rng() % 16];
    return s;
}

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string now_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string url_decode(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            char h[3] = {in[i+1], in[i+2], 0};
            out += (char)std::strtol(h, nullptr, 16);
            i += 2;
        } else if (in[i] == '+') out += ' ';
        else out += in[i];
    }
    return out;
}

std::map<std::string,std::string> parse_form(const std::string& body) {
    std::map<std::string,std::string> m;
    std::stringstream ss(body);
    std::string part;
    while (std::getline(ss, part, '&')) {
        auto p = part.find('=');
        if (p != std::string::npos)
            m[url_decode(part.substr(0,p))] = url_decode(part.substr(p+1));
    }
    return m;
}

void save_profile(const BagProfile& b) {
    fs::create_directories(PROFILE_DIR);
    std::ofstream f(PROFILE_DIR + "/" + b.bag_id + ".txt", std::ios::trunc);
    f << "bag_id=" << b.bag_id << "\n";
    f << "student_name=" << b.student_name << "\n";
    f << "student_id=" << b.student_id << "\n";
    f << "phone=" << b.phone << "\n";
    f << "message=" << b.message << "\n";
    f << "photo_data=" << b.photo_data << "\n";
    f << "owner_token=" << b.owner_token << "\n";
}

bool load_profile(const std::string& bag_id, BagProfile& b) {
    std::ifstream f(PROFILE_DIR + "/" + bag_id + ".txt");
    if (!f) return false;
    std::string line;
    std::map<std::string,std::string> v;
    while (std::getline(f,line)) {
        auto p = line.find('=');
        if (p != std::string::npos) v[line.substr(0,p)] = line.substr(p+1);
    }
    b.bag_id = v["bag_id"];
    b.student_name = v["student_name"];
    b.student_id = v["student_id"];
    b.phone = v["phone"];
    b.message = v["message"];
    b.photo_data = v["photo_data"];
    b.owner_token = v["owner_token"];
    return !b.bag_id.empty();
}

void save_report(const FinderReport& r) {
    fs::create_directories(REPORT_DIR);
    std::ofstream f(REPORT_DIR + "/" + r.report_id + ".txt");
    f << "bag_id=" << r.bag_id << "\n";
    f << "latitude=" << std::setprecision(12) << r.latitude << "\n";
    f << "longitude=" << std::setprecision(12) << r.longitude << "\n";
    f << "timestamp=" << r.timestamp << "\n";
}

std::vector<FinderReport> load_reports(const std::string& bag_id) {
    std::vector<FinderReport> v;
    if (!fs::exists(REPORT_DIR)) return v;
    for (auto& e : fs::directory_iterator(REPORT_DIR)) {
        if (!e.is_regular_file()) continue;
        std::ifstream f(e.path());
        std::map<std::string,std::string> x;
        std::string line;
        while (std::getline(f,line)) {
            auto p=line.find('=');
            if(p!=std::string::npos) x[line.substr(0,p)] = line.substr(p+1);
        }
        if (x["bag_id"] != bag_id) continue;
        FinderReport r{};
        std::snprintf(r.bag_id, sizeof(r.bag_id), "%s", x["bag_id"].c_str());
        r.latitude = std::stod(x["latitude"]);
        r.longitude = std::stod(x["longitude"]);
        r.timestamp = std::stoll(x["timestamp"]);
        std::snprintf(r.report_id, sizeof(r.report_id), "%s", e.path().stem().string().c_str());
        v.push_back(r);
    }
    std::sort(v.begin(), v.end(), [](auto&a,auto&b){return a.timestamp>b.timestamp;});
    return v;
}

std::string static_file(const std::string& path, std::string& mime) {
    std::string p = path;
    if (p == "/") p = "/index.html";
    if (p.rfind("/assets/",0)==0) p = "/public" + p;
    else p = "/public" + p;
    fs::path fp = fs::path(".") / p.substr(1);
    if (!fs::exists(fp) || !fs::is_regular_file(fp)) return "";
    auto ext = fp.extension().string();
    mime = "text/plain";
    if (ext==".html") mime="text/html; charset=utf-8";
    else if (ext==".css") mime="text/css; charset=utf-8";
    else if (ext==".js") mime="application/javascript; charset=utf-8";
    else if (ext==".svg") mime="image/svg+xml";
    else if (ext==".png") mime="image/png";
    else if (ext==".jpg" || ext==".jpeg") mime="image/jpeg";
    std::ifstream f(fp, std::ios::binary);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}

std::string http_response(const std::string& body, const std::string& type="text/html; charset=utf-8", int code=200) {
    std::string status;
    switch (code) {
        case 200: status = "200 OK"; break;
        case 400: status = "400 Bad Request"; break;
        case 401: status = "401 Unauthorized"; break;
        case 404: status = "404 Not Found"; break;
        default: status = std::to_string(code) + " Error"; break;
    }
    std::ostringstream o;
    o << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << type << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Cache-Control: no-store\r\nConnection: close\r\n\r\n" << body;
    return o.str();
}

void send_all(socket_t s, const std::string& data) {
    size_t sent=0;
    while(sent<data.size()) {
#ifdef _WIN32
        int n=send(s,data.data()+sent,(int)(data.size()-sent),0);
#else
        ssize_t n=send(s,data.data()+sent,data.size()-sent,0);
#endif
        if(n<=0) break;
        sent += (size_t)n;
    }
}

std::string profile_json(const BagProfile& b, const std::vector<FinderReport>& reports) {
    std::ostringstream o;
    o << "{\"bagId\":\""<<json_escape(b.bag_id)<<"\","
      << "\"studentName\":\""<<json_escape(b.student_name)<<"\","
      << "\"studentId\":\""<<json_escape(b.student_id)<<"\","
      << "\"phone\":\""<<json_escape(b.phone)<<"\","
      << "\"message\":\""<<json_escape(b.message)<<"\","
      << "\"photo\":\""<<json_escape(b.photo_data)<<"\","
      << "\"ownerToken\":\""<<json_escape(b.owner_token)<<"\","
      << "\"reports\":[";
    for(size_t i=0;i<reports.size();++i){
        if(i) o<<",";
        o<<"{\"id\":\""<<json_escape(reports[i].report_id)<<"\","
         <<"\"latitude\":"<<std::setprecision(12)<<reports[i].latitude<<","
         <<"\"longitude\":"<<reports[i].longitude<<","
         <<"\"timestamp\":"<<reports[i].timestamp<<"}";
    }
    o<<"]}";
    return o.str();
}

std::string url_query_param(const std::string& full_path, const std::string& key) {
    auto q = full_path.find('?');
    if (q == std::string::npos) return "";
    auto m = parse_form(full_path.substr(q + 1));
    return m[key];
}

bool admin_authorized(const std::string& token) {
    if (token.empty()) return false;
    std::lock_guard<std::mutex> lock(admin_mutex);
    return admin_tokens.count(token) > 0;
}

std::vector<std::string> list_bag_ids() {
    std::vector<std::string> ids;
    if (!fs::exists(PROFILE_DIR)) return ids;
    for (auto& e : fs::directory_iterator(PROFILE_DIR)) {
        if (e.is_regular_file() && e.path().extension() == ".txt")
            ids.push_back(e.path().stem().string());
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::string admin_bags_json() {
    auto ids = list_bag_ids();
    long long total_reports = 0;
    std::ostringstream o;
    o << "{\"bags\":[";
    bool first = true;
    for (auto& id : ids) {
        BagProfile b;
        if (!load_profile(id, b)) continue;
        auto reports = load_reports(id);
        total_reports += (long long)reports.size();
        long long latest = reports.empty() ? 0 : reports[0].timestamp;
        if (!first) o << ",";
        first = false;
        o << "{\"bagId\":\"" << json_escape(b.bag_id) << "\","
          << "\"studentName\":\"" << json_escape(b.student_name) << "\","
          << "\"studentId\":\"" << json_escape(b.student_id) << "\","
          << "\"phone\":\"" << json_escape(b.phone) << "\","
          << "\"reportCount\":" << reports.size() << ","
          << "\"latestReport\":" << latest << "}";
    }
    o << "],\"totalBags\":" << ids.size() << ",\"totalReports\":" << total_reports << "}";
    return o.str();
}

void delete_bag_and_reports(const std::string& id) {
    fs::path pf = fs::path(PROFILE_DIR) / (id + ".txt");
    fs::remove(pf);
    if (!fs::exists(REPORT_DIR)) return;
    for (auto& e : fs::directory_iterator(REPORT_DIR)) {
        if (!e.is_regular_file()) continue;
        std::ifstream f(e.path());
        std::map<std::string,std::string> x;
        std::string line;
        while (std::getline(f, line)) {
            auto p = line.find('=');
            if (p != std::string::npos) x[line.substr(0,p)] = line.substr(p+1);
        }
        f.close();
        if (x["bag_id"] == id) fs::remove(e.path());
    }
}

std::string handle_request(const std::string& req) {
    auto first_end=req.find("\r\n");
    if(first_end==std::string::npos) return http_response("Bad request","text/plain",400);
    std::string first=req.substr(0,first_end);
    std::stringstream fsx(first);
    std::string method,path,version;
    fsx>>method>>path>>version;

    // Strip the query string for file/API routing. The browser still keeps
    // ?bag=...&token=... in the URL so dashboard.js can read it.
    const std::string::size_type query_pos = path.find('?');
    const std::string request_path = (query_pos == std::string::npos)
        ? path
        : path.substr(0, query_pos);

    std::string body;
    auto header_end=req.find("\r\n\r\n");
    if(header_end!=std::string::npos) body=req.substr(header_end+4);

    if(method=="GET") {
        if(request_path.rfind("/api/bag/",0)==0) {
            std::string id=request_path.substr(9);
            BagProfile b;
            if(!load_profile(id,b)) return http_response("{\"error\":\"Bag not found\"}","application/json",404);
            auto reports=load_reports(id);
            return http_response(profile_json(b,reports),"application/json");
        }

        if(request_path == "/api/admin/bags") {
            std::string token = url_query_param(path, "token");
            if (!admin_authorized(token)) return http_response("{\"error\":\"Unauthorized\"}","application/json",401);
            return http_response(admin_bags_json(),"application/json");
        }

        if(request_path.rfind("/api/admin/bag/",0)==0) {
            std::string token = url_query_param(path, "token");
            if (!admin_authorized(token)) return http_response("{\"error\":\"Unauthorized\"}","application/json",401);
            std::string id = request_path.substr(15);
            BagProfile b;
            if (!load_profile(id,b)) return http_response("{\"error\":\"Bag not found\"}","application/json",404);
            auto reports = load_reports(id);
            return http_response(profile_json(b,reports),"application/json");
        }

        // Friendly application routes. These must map to the actual HTML files.
        if(request_path == "/dashboard") {
            std::string mime;
            std::string data=static_file("/dashboard.html",mime);
            return http_response(data,mime);
        }

        if(request_path == "/admin") {
            std::string mime;
            std::string data=static_file("/admin.html",mime);
            return http_response(data,mime);
        }

        if(request_path == "/my") {
            std::string mime;
            std::string data=static_file("/my.html",mime);
            return http_response(data,mime);
        }

        // Every NFC tag has its own public URL, but all tag pages use finder.html.
        if(request_path.rfind("/bag/",0)==0) {
            std::string mime;
            std::string data=static_file("/finder.html",mime);
            return http_response(data,mime);
        }

        std::string mime;
        std::string data=static_file(request_path,mime);
        if(data.empty()) return http_response("<h1>404</h1><p>Page not found.</p>","text/html",404);
        return http_response(data,mime);
    }

    if(method=="POST" && path=="/api/register") {
        auto m=parse_form(body);
        if(m["student_name"].empty() || m["student_id"].empty() || m["phone"].empty())
            return http_response("{\"error\":\"Name, student ID and phone are required.\"}","application/json",400);

        BagProfile b;
        b.bag_id=random_id("BG-",4);
        b.student_name=m["student_name"];
        b.student_id=m["student_id"];
        b.phone=m["phone"];
        b.message=m["message"];
        b.photo_data=m["photo_data"];
        b.owner_token=random_id("OWN-",8);

        std::lock_guard<std::mutex> lock(data_mutex);
        save_profile(b);

        std::ostringstream o;
        o<<"{\"bagId\":\""<<b.bag_id<<"\",\"ownerToken\":\""<<b.owner_token
         <<"\",\"url\":\"http://localhost:8000/bag/"<<b.bag_id
         <<"\",\"dashboard\":\"/dashboard?bag="<<b.bag_id<<"&token="<<b.owner_token<<"\"}";
        return http_response(o.str(),"application/json");
    }

    if(method=="POST" && path=="/api/report") {
        auto m=parse_form(body);
        BagProfile b;
        if(!load_profile(m["bag_id"],b))
            return http_response("{\"error\":\"Bag not found.\"}","application/json",404);

        if(m["latitude"].empty() || m["longitude"].empty())
            return http_response("{\"error\":\"Location is required.\"}","application/json",400);

        FinderReport r{};
        std::snprintf(r.bag_id,sizeof(r.bag_id),"%s",m["bag_id"].c_str());
        r.latitude=std::stod(m["latitude"]);
        r.longitude=std::stod(m["longitude"]);
        r.timestamp=now_ms();
        std::string rid=random_id("RP-",6);
        std::snprintf(r.report_id,sizeof(r.report_id),"%s",rid.c_str());

        std::lock_guard<std::mutex> lock(data_mutex);
        queue_push(&incoming_reports,&r);

        /* FIFO processing: oldest incoming report is persisted first. */
        FinderReport next{};
        while(queue_pop(&incoming_reports,&next)) save_report(next);

        return http_response("{\"ok\":true,\"message\":\"Location shared with the bag owner.\"}","application/json");
    }

    if(method=="POST" && request_path=="/api/admin/login") {
        auto m=parse_form(body);
        if(m["password"] != ADMIN_PASSWORD)
            return http_response("{\"error\":\"Incorrect admin password.\"}","application/json",401);

        std::string token = random_id("ADM-",12);
        { std::lock_guard<std::mutex> lock(admin_mutex); admin_tokens.insert(token); }
        return http_response("{\"ok\":true,\"token\":\""+token+"\"}","application/json");
    }

    if(method=="POST" && request_path=="/api/admin/logout") {
        auto m=parse_form(body);
        { std::lock_guard<std::mutex> lock(admin_mutex); admin_tokens.erase(m["token"]); }
        return http_response("{\"ok\":true}","application/json");
    }

    if(method=="POST" && request_path=="/api/admin/delete") {
        auto m=parse_form(body);
        if(!admin_authorized(m["token"]))
            return http_response("{\"error\":\"Unauthorized\"}","application/json",401);

        std::string id=m["bag_id"];
        if(id.empty()) return http_response("{\"error\":\"Missing bag_id.\"}","application/json",400);

        std::lock_guard<std::mutex> lock(data_mutex);
        BagProfile b;
        if(!load_profile(id,b)) return http_response("{\"error\":\"Bag not found.\"}","application/json",404);
        delete_bag_and_reports(id);
        return http_response("{\"ok\":true}","application/json");
    }

    return http_response("Method not allowed","text/plain",400);
}

void client_thread(socket_t client) {
    char buf[8192];
    std::string req;
    while(req.find("\r\n\r\n")==std::string::npos && req.size()<1024*1024) {
#ifdef _WIN32
        int n=recv(client,buf,sizeof(buf),0);
#else
        ssize_t n=recv(client,buf,sizeof(buf),0);
#endif
        if(n<=0) break;
        req.append(buf,n);
    }

    auto header_end=req.find("\r\n\r\n");
    if(header_end!=std::string::npos) {
        std::string headers=req.substr(0,header_end);
        size_t pos=headers.find("Content-Length:");
        if(pos!=std::string::npos) {
            size_t e=headers.find("\r\n",pos);
            std::string v=headers.substr(pos+15,e-pos-15);
            size_t len=std::stoul(v);
            while(req.size()-header_end-4 < len) {
#ifdef _WIN32
                int n=recv(client,buf,sizeof(buf),0);
#else
                ssize_t n=recv(client,buf,sizeof(buf),0);
#endif
                if(n<=0) break;
                req.append(buf,n);
            }
        }
    }

    send_all(client,handle_request(req));
    close_socket(client);
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0) return 1;
#endif

    fs::create_directories(PROFILE_DIR);
    fs::create_directories(REPORT_DIR);
    queue_init(&incoming_reports);

    socket_t server=socket(AF_INET,SOCK_STREAM,0);
    if(server==INVALID_SOCKET) {
        std::cerr<<"Could not create socket.\n";
        return 1;
    }

    int opt=1;
#ifdef _WIN32
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));
#else
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(PORT);

    if(bind(server,(sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR) {
        std::cerr<<"Could not bind to port "<<PORT<<".\n";
        close_socket(server);
        return 1;
    }

    if(listen(server,16)==SOCKET_ERROR) {
        std::cerr<<"Could not listen.\n";
        close_socket(server);
        return 1;
    }

    std::cout<<"BagSphere server running at http://localhost:8000\n";
    std::cout<<"Admin panel: http://localhost:8000/admin  (password: "<<ADMIN_PASSWORD<<")\n";
    std::cout<<"Set BAGSPHERE_ADMIN_PASSWORD to change it.\n";
    std::cout<<"Press Ctrl+C to stop.\n";

    while(true) {
        sockaddr_in client_addr{};
        socklen_type len=sizeof(client_addr);
        socket_t client=accept(server,(sockaddr*)&client_addr,&len);
        if(client==INVALID_SOCKET) continue;
        std::thread(client_thread,client).detach();
    }

    close_socket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
