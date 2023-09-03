#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace YS
{


class httpHeader
{
public:
    enum class HeaderStatus : uint8_t
    {
        AnalysisCompleteNo  =   0,
        AnalysisCompleteOK  =   1,
        HeaderFormatError   =   2,
    };

    enum class ReqMethod : uint8_t
    {
        No   =   0,

        GET,
        POST,

        Max
    };

    enum class ReqType : uint8_t
    {
        NO  =   0,

        HTTP,
        HTTPS,
        WS,

        MAX
    };

    struct HeaderReqLine
    {
        ReqMethod       method;
        std::string     url;
        ReqType         type;
        float           ver;
    };

public:
    httpHeader();
    httpHeader(const char* httpHeaderStr_);
    httpHeader(const std::string& httpHeaderStr_);
    httpHeader(httpHeader&&);
    httpHeader(const httpHeader&);

public:
    bool IsAnalysisComplete();
    HeaderStatus Append(const std::string& headerStr_);
    HeaderStatus Append(const char* headerCstr_);

private:
    // str_ : 要解析的字符串    splistStr_ : 分割字符串     reVec_ : 接收的容器     return : 分割出的数量
    size_t Splist(const std::string& str_, const std::string& splistStr_, std::vector<std::string>& reVec_);
    HeaderStatus Parser();

private:
    bool reqParserStatus;
    bool headersParserStatus;
    bool contentParserStatus;
    static const std::string        splistStr;          // 间隔符号
    std::string                     headerStr;          // 用于保存要解析的字符串
    size_t                          headerContentSize;  // Content-Lengh数据大小
    std::vector<std::string>        headerVec;          // 解析出来的一行一行的数据

private:
    HeaderReqLine                                   req;
    std::unordered_map<std::string, std::string>    headers;
    std::string                                     content;
};


};


