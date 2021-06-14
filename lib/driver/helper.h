#include <vector>
#include <string>
#include <iostream>
#include <sstream>

template <typename T>
std::string str_join(const T &v, const std::string &delim)
{
    std::ostringstream s;
    for (const auto &i : v)
    {
        if (&i != &v[0])
        {
            s << delim;
        }
        s << i;
    }
    return s.str();
}

std::vector<std::string> str_split(const std::string &str, const std::string &delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos)
            pos = str.length();
        std::string token = str.substr(prev, pos - prev);
        if (!token.empty())
            tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

#define TF_ROCM_VERSION 42000
std::string MapGCNArchNameTokenToFeatureStr(const std::string &token)
{
    if (token == "sramecc+")
    {
        return "+sramecc";
    }
    else if (token == "sramecc-")
    {
#if TF_ROCM_VERSION < 40100
        return "";
#else
        return "-sramecc";
#endif
    }
    else if (token == "xnack+")
    {
        return "+xnack";
    }
    else if (token == "xnack-")
    {
        return "-xnack";
    }
    return "";
}

std::pair<std::string, std::string> GetFeatureStrFromGCNArchName(
    const std::string &gcn_arch_name)
{
    std::string feature_str;
    std::string gfx = gcn_arch_name;
#if TF_ROCM_VERSION < 30900
    // For ROCm versions older than 3.9, hardcode it to "+code-object-v3"
    // This is simply to preserve how things were...nohing else
    feature_str = "+code-object-v3";
#elif TF_ROCM_VERSION < 40000
    // For ROCM versions 3.9 and 3.10, hardcode it to empty string
    feature_str = "";
#else
    // For ROCm versions 4.0 and greater, we need to specify the correct
    // feature str, based on the underlying GPU HW to get max performance.
    // std::vector<std::string> tokens = absl::StrSplit(gcn_arch_name, ':');
    std::vector<std::string> tokens = str_split(gcn_arch_name, ":");
    std::vector<std::string> mapped_tokens;
    if (tokens.size() > 0)
        gfx = tokens[0];
    for (auto it = tokens.begin(); it != tokens.end(); it++)
    {
        // Skip the first token, that is the gfxNNN str
        // The rest of the tokens are the feature/targetid strings
        if (it != tokens.begin())
        {
            std::string token(*it);
            std::string mapped_token = MapGCNArchNameTokenToFeatureStr(token);
            mapped_tokens.push_back(mapped_token);
        }
    }
    // feature_str = absl::StrJoin(mapped_tokens, ",");
    feature_str = str_join(mapped_tokens, ",");
    std::cout << "feature_str: " << feature_str << std::endl;
#endif

    return make_pair(gfx, feature_str);
}