#include "SocialHistoryCrypto.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <memory>

namespace auriga::social::history {
std::string Hex(std::string_view bytes){static constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(bytes.size()*2);for(unsigned char c:bytes){result+=digits[c>>4];result+=digits[c&15];}return result;}
std::optional<std::string> Unhex(std::string_view text){if(text.size()%2)return {};auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};std::string result;result.reserve(text.size()/2);for(size_t i=0;i<text.size();i+=2){int a=digit(text[i]),b=digit(text[i+1]);if(a<0||b<0)return {};result+=static_cast<char>(a*16+b);}return result;}
bool Valid(const Metadata& m){return m.id>0&&m.id<9223372036854775807ULL&&m.created>0&&m.created<=253402300799000ULL&&m.sender>0&&!m.name.empty()&&m.name.size()<=24&&m.empire<=3&&m.gm<=1&&((m.kind==1&&m.a>0&&m.a<m.b&&(m.sender==m.a||m.sender==m.b))||(m.kind==2&&m.a>0&&m.b==0));}
static std::string Aad(const Metadata& m){std::string out="Auriga.SocialHistory.v1";auto number=[&](std::uint64_t n){for(int i=0;i<8;i++)out+=static_cast<char>(n>>(i*8));};number(m.id);number(m.created);number(m.kind);number(m.a);number(m.b);number(m.sender);number(m.empire);number(m.gm);number(m.name.size());out+=m.name;return out;}
using Context=std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)>;
std::optional<Sealed> Seal(const Key& key,const Metadata& metadata,std::string_view text)
{
    if(!Valid(metadata)||text.empty()||text.size()>1024||text.find('\0')!=std::string_view::npos)return {};
    Context context(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);if(!context)return {};Sealed out;out.nonce.resize(12);out.tag.resize(16);out.cipher.resize(text.size()+16);int length=0,total=0;auto aad=Aad(metadata);
    if(RAND_bytes(reinterpret_cast<unsigned char*>(out.nonce.data()),12)!=1||EVP_EncryptInit_ex(context.get(),EVP_aes_256_gcm(),nullptr,key.data(),reinterpret_cast<const unsigned char*>(out.nonce.data()))!=1||EVP_EncryptUpdate(context.get(),nullptr,&length,reinterpret_cast<const unsigned char*>(aad.data()),static_cast<int>(aad.size()))!=1||EVP_EncryptUpdate(context.get(),reinterpret_cast<unsigned char*>(out.cipher.data()),&length,reinterpret_cast<const unsigned char*>(text.data()),static_cast<int>(text.size()))!=1)return {};
    total=length;if(EVP_EncryptFinal_ex(context.get(),reinterpret_cast<unsigned char*>(out.cipher.data())+total,&length)!=1||EVP_CIPHER_CTX_ctrl(context.get(),EVP_CTRL_GCM_GET_TAG,16,out.tag.data())!=1)return {};out.cipher.resize(total+length);return out;
}
std::optional<std::string> Open(const Key& key,const Metadata& metadata,const Sealed& sealed)
{
    if(!Valid(metadata)||sealed.nonce.size()!=12||sealed.tag.size()!=16||sealed.cipher.empty()||sealed.cipher.size()>1024)return {};
    Context context(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);if(!context)return {};auto aad=Aad(metadata);std::string text(sealed.cipher.size()+16,'\0');int length=0,total=0;
    if(EVP_DecryptInit_ex(context.get(),EVP_aes_256_gcm(),nullptr,key.data(),reinterpret_cast<const unsigned char*>(sealed.nonce.data()))!=1||EVP_DecryptUpdate(context.get(),nullptr,&length,reinterpret_cast<const unsigned char*>(aad.data()),static_cast<int>(aad.size()))!=1||EVP_DecryptUpdate(context.get(),reinterpret_cast<unsigned char*>(text.data()),&length,reinterpret_cast<const unsigned char*>(sealed.cipher.data()),static_cast<int>(sealed.cipher.size()))!=1)return {};
    total=length;if(EVP_CIPHER_CTX_ctrl(context.get(),EVP_CTRL_GCM_SET_TAG,16,const_cast<char*>(sealed.tag.data()))!=1||EVP_DecryptFinal_ex(context.get(),reinterpret_cast<unsigned char*>(text.data())+total,&length)!=1){OPENSSL_cleanse(text.data(),text.size());return {};}text.resize(total+length);return text;
}
}
