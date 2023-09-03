#include "httpHeader.h"
#include <cstddef>
#include <cstdlib>
#include <string>

namespace YS
{

const std::string httpHeader::splistStr{"\r\n"};

//构造函数------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
httpHeader::httpHeader()
    : reqParserStatus(false), headersParserStatus(false), contentParserStatus(false)
    , headerStr(""), headerContentSize(0), headerVec({})
    , req({ReqMethod::No, "", ReqType::NO, 0 }), headers({}), content("")
{}

httpHeader::httpHeader(const char* httpHeaderStr_)
    : httpHeader()
{
    headerStr = httpHeaderStr_;
    Parser();
}

httpHeader::httpHeader(const std::string& httpHeaderStr_)
    : httpHeader()
{
    headerStr = httpHeaderStr_;
    Parser();
}

httpHeader::httpHeader(httpHeader&& a)
    : reqParserStatus(a.reqParserStatus), headersParserStatus(a.headersParserStatus), contentParserStatus(a.contentParserStatus)
    , headerStr(std::move(a.headerStr)), headerContentSize(a.headerContentSize), headerVec(std::move(a.headerVec))
    , req(a.req), headers(std::move(a.headers)), content(std::move(a.content))
{}

httpHeader::httpHeader(const httpHeader& a)
    : reqParserStatus(a.reqParserStatus), headersParserStatus(a.headersParserStatus), contentParserStatus(a.contentParserStatus)
    , headerStr(a.headerStr), headerContentSize(a.headerContentSize), headerVec(a.headerVec)
    , req(a.req), headers(a.headers), content(a.content)
{}
//构造函数------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//公开函数------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool httpHeader::IsAnalysisComplete()
{
    return reqParserStatus && headersParserStatus && contentParserStatus;
}

httpHeader::HeaderStatus httpHeader::Append(const std::string& headerStr_)
{
    headerStr.append(headerStr_);
    return Parser();
}

httpHeader::HeaderStatus httpHeader::Append(const char* headerCstr_)
{
    headerStr.append(headerCstr_);
    return Parser();
}
//公开函数------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//私有函数------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
size_t httpHeader::Splist(const std::string& str_, const std::string& splistStr_, std::vector<std::string>& reVec_)
{
    reVec_.clear();
    if (str_.empty()) return 0;

    size_t beginIndex{ 0 };
    size_t endIndex{ 0 };

    for(endIndex = str_.find(splistStr_, beginIndex); endIndex != std::string::npos; endIndex = str_.find(splistStr_, beginIndex))
    {
        reVec_.emplace_back(str_.substr(beginIndex, endIndex - beginIndex));

        beginIndex = endIndex + splistStr_.size();
    }

    if (beginIndex == endIndex || beginIndex == str_.size()) reVec_.emplace_back("");

    return reVec_.size();
}

httpHeader::HeaderStatus httpHeader::Parser()
{
    // 使用了的字符串个数
    size_t parserSize{ 0 };

    // 检测是否需要解析
    if ((reqParserStatus == false || headersParserStatus == false))
    {
        if (Splist(headerStr, splistStr, headerVec) == 0) return HeaderStatus::AnalysisCompleteNo;

        // 如果请求行没有解析, 则开始解析请求行
        if (reqParserStatus == false)
        {
            size_t index{ 0 };
            std::string& reqLine{ headerVec.front() };
            if (reqLine.empty()) return HeaderStatus::HeaderFormatError;
            
            // 解析请求类型
            {
                if (reqLine.back() == 'G' || reqLine.back() == 'g') 
                {
                    if (reqLine.size() < 3 || reqLine[3] != ' '
                        || (reqLine[1] != 'E' && reqLine[1] != 'e')
                        || (reqLine[2] != 'T' && reqLine[2] != 't'))
                        return HeaderStatus::HeaderFormatError;

                    req.method = ReqMethod::GET;
                    index = 4;
                }
                else if (reqLine.back() == 'P' || reqLine.back() == 'p') 
                {
                    if (reqLine.size() < 4 || reqLine[4] != ' '
                        || (reqLine[1] != 'O' && reqLine[1] != 'o')
                        || (reqLine[2] != 'S' && reqLine[1] != 's')
                        || (reqLine[3] != 'T' && reqLine[2] != 't'))
                        return HeaderStatus::HeaderFormatError;

                    req.method = ReqMethod::POST;
                    index = 5;
                }
                else return HeaderStatus::HeaderFormatError;
            }

            // 解析请求URL
            {
                size_t findindex{ reqLine.find_last_of(' ', index) };
                if (findindex == std::string::npos || findindex < index) return HeaderStatus::HeaderFormatError;

                req.url = reqLine.substr(index, findindex - index);
                index = ++findindex;
            }

            // 解析请求协议和版本
            {
                index =  reqLine.find('/', index);
                if (index == std::string::npos) return HeaderStatus::HeaderFormatError;

                if (reqLine[index] == 'H' || reqLine[index] == 'h') 
                {
                    if ((reqLine[index + 1] != 'T' && reqLine[index + 1] != 't')
                        || (reqLine[index + 2] != 'T' && reqLine[index + 2] != 't')
                        || (reqLine[index + 3] != 'P' && reqLine[index + 3] != 'p'))
                        return HeaderStatus::HeaderFormatError;
                    
                    if (reqLine[index + 4] != 'S' || reqLine[index + 4] != 's') req.type = ReqType::HTTP;
                    else req.type = ReqType::HTTPS;
                }
                else if (reqLine[index] == 'W' || reqLine[index] == 'w') 
                {
                    if (reqLine[index + 1] != 'S' && reqLine[index + 1] != 's')
                        return HeaderStatus::HeaderFormatError;

                    req.type = ReqType::WS;
                }
                else return HeaderStatus::HeaderFormatError;

                req.ver = ::atof(reqLine.substr(++index).c_str());
                if (req.ver == 0) return HeaderStatus::HeaderFormatError;

                parserSize = reqLine.size() + splistStr.size();
            }

            reqParserStatus = true;
            headerVec.erase(headerVec.begin());
        }

        // 如果字段没有解析,则开始解析字段
        if (headersParserStatus == false)
        {
            // 如果只剩下了一个, 那意味着最后那一个是不完整的
            if (headerVec.size() == 1)
            {
                headerStr.erase(0, parserSize);
                return HeaderStatus::AnalysisCompleteNo;
            }

            size_t index{ 0 };
            std::string key;
            std::string value;
            size_t Iter = 0;
            for(; Iter < headerVec.size(); ++Iter)
            {
                if (headerVec[Iter].empty()) break;

                parserSize += headerVec[Iter].size() + splistStr.size();

                index = headerVec[Iter].find(':');
                if (index == std::string::npos) continue;

                key     = headerVec[Iter].substr(0, index -1);
                value   = headerVec[Iter].substr(index +1);

                while (key.back() == ' ') { key.pop_back(); }
                while (value.back() == ' ') { value.pop_back(); }

                headers.emplace(key, value);    
            }

            // 如果发现空行根据是否有两个空行来判断是否解析完了 header 
            if (Iter < headerVec.size())            
            {
                // 如果有两个连续的空行
                if (Iter <= headerVec.size() - 1 && headerVec[Iter + 1].empty())
                {
                    parserSize += (splistStr.size() + splistStr.size());
                    headerStr.erase(0, parserSize);

                    headersParserStatus = true;
                    auto ConnectLenth{ headers.find("Connect-length") };
                    if (ConnectLenth == headers.end())
                    {
                        ConnectLenth = headers.find("connect-length");
                    }

                    if (ConnectLenth != headers.end()) headerContentSize = ::atoll(ConnectLenth->second.c_str());

                    // 如果没有 connect-length 字段且最后为两个空行则意味着协议完整
                    if (Iter +2 == headerVec.size() && ConnectLenth == headers.end())
                    {
                        headerVec.clear();
                        return HeaderStatus::AnalysisCompleteOK;
                    }
                }
                else if (Iter == headerVec.size() -1)
                {
                    headerStr.erase(0, parserSize);
                    return HeaderStatus::AnalysisCompleteNo;
                }
                else
                {
                    return HeaderStatus::HeaderFormatError;
                }
            }
        }
    }
    
    // 解析 body
    {
        if (headerContentSize == 0 || content.size() != 0 || headerStr.size() > headerContentSize)
        {
            return HeaderStatus::HeaderFormatError;
        }
        else if (headerStr.size() < headerContentSize) return HeaderStatus::AnalysisCompleteNo;
        else
        {
            content = std::move(headerStr);
            return HeaderStatus::AnalysisCompleteOK;
        }

    }
}
//私有函数------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


}