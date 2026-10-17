// Hangul automaton and scoring approach adapted from Seamtype (MIT).
// See third_party/seamtype/LICENSE and THIRD_PARTY_NOTICES.md.
#include "engine.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace atypoi {
std::wstring fromUtf8(std::string_view s) {
    std::wstring out;
    for (size_t i=0; i<s.size();) {
        unsigned c=static_cast<unsigned char>(s[i++]);
        int n=0;
        if (c>=0xf0) { c &= 7; n=3; } else if(c>=0xe0) { c &= 15; n=2; }
        else if(c>=0xc0) { c &= 31; n=1; }
        else if(c>=0x80) throw std::runtime_error("Invalid UTF-8");
        while(n--) { if(i>=s.size() || (static_cast<unsigned char>(s[i]) & 0xc0)!=0x80) throw std::runtime_error("Invalid UTF-8"); c=(c<<6)|(s[i++]&63); }
        if(c>0x10ffff || (c>=0xd800 && c<=0xdfff)) throw std::runtime_error("Invalid Unicode");
        if(c>0xffff && sizeof(wchar_t)==2) { c-=0x10000; out.push_back(static_cast<wchar_t>(0xd800+(c>>10))); out.push_back(static_cast<wchar_t>(0xdc00+(c&1023))); }
        else out.push_back(static_cast<wchar_t>(c));
    }
    return out;
}
std::string toUtf8(std::wstring_view s) {
    std::string out;
    for(size_t i=0; i<s.size(); ++i) {
        unsigned c=s[i];
        if(c>=0xd800 && c<=0xdbff && i+1<s.size()) { unsigned d=s[++i]; c=0x10000+((c-0xd800)<<10)+(d-0xdc00); }
        if(c<128) out.push_back(static_cast<char>(c));
        else if(c<2048) { out.push_back(static_cast<char>(0xc0|(c>>6))); out.push_back(static_cast<char>(0x80|(c&63))); }
        else if(c<65536) { out.push_back(static_cast<char>(0xe0|(c>>12))); out.push_back(static_cast<char>(0x80|((c>>6)&63))); out.push_back(static_cast<char>(0x80|(c&63))); }
        else { out.push_back(static_cast<char>(0xf0|(c>>18))); out.push_back(static_cast<char>(0x80|((c>>12)&63))); out.push_back(static_cast<char>(0x80|((c>>6)&63))); out.push_back(static_cast<char>(0x80|(c&63))); }
    }
    return out;
}
namespace {
constexpr std::wstring_view initials=L"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ";
constexpr std::wstring_view vowels=L"ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ";
constexpr std::wstring_view finals=L" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ";
constexpr std::wstring_view layout=L"ㅁㅠㅊㅇㄷㄹㅎㅗㅑㅓㅏㅣㅡㅜㅐㅔㅂㄱㄴㅅㅕㅍㅈㅌㅛㅋ";
struct Pair { wchar_t a,b,c; };
constexpr Pair vowelPairs[]={{L'ㅗ',L'ㅏ',L'ㅘ'},{L'ㅗ',L'ㅐ',L'ㅙ'},{L'ㅗ',L'ㅣ',L'ㅚ'},{L'ㅜ',L'ㅓ',L'ㅝ'},{L'ㅜ',L'ㅔ',L'ㅞ'},{L'ㅜ',L'ㅣ',L'ㅟ'},{L'ㅡ',L'ㅣ',L'ㅢ'}};
constexpr Pair finalPairs[]={{L'ㄱ',L'ㅅ',L'ㄳ'},{L'ㄴ',L'ㅈ',L'ㄵ'},{L'ㄴ',L'ㅎ',L'ㄶ'},{L'ㄹ',L'ㄱ',L'ㄺ'},{L'ㄹ',L'ㅁ',L'ㄻ'},{L'ㄹ',L'ㅂ',L'ㄼ'},{L'ㄹ',L'ㅅ',L'ㄽ'},{L'ㄹ',L'ㅌ',L'ㄾ'},{L'ㄹ',L'ㅍ',L'ㄿ'},{L'ㄹ',L'ㅎ',L'ㅀ'},{L'ㅂ',L'ㅅ',L'ㅄ'}};
template<size_t N> wchar_t combine(wchar_t a,wchar_t b,const Pair(&pairs)[N]) { for(auto p:pairs) if(p.a==a && p.b==b) return p.c; return 0; }
wchar_t jamo(char c) {
    switch(c) { case 'Q':return L'ㅃ'; case 'W':return L'ㅉ'; case 'E':return L'ㄸ'; case 'R':return L'ㄲ'; case 'T':return L'ㅆ'; case 'O':return L'ㅒ'; case 'P':return L'ㅖ'; }
    if(c>='A' && c<='Z') c+=32;
    return c>='a' && c<='z' ? layout[c-'a'] : 0;
}
bool isSyllable(wchar_t c) { return c>=0xac00 && c<=0xd7a3; }
bool hasOrdinaryCapital(std::string_view keys) {return std::any_of(keys.begin(),keys.end(),[](char c){return c>='A'&&c<='Z'&&std::string_view("QWERTOP").find(c)==std::string_view::npos;});}
bool koreanCase(std::string_view keys) {
    bool shiftedRun=false;
    for(char c:keys) {
        if(c>='A'&&c<='Z') {
            if(std::string_view("QWERTOP").find(c)!=std::string_view::npos)shiftedRun=true;
            else if(!shiftedRun)return false;
        } else shiftedRun=false;
    }
    return true;
}
std::string lower(std::string_view text) { std::string s(text); for(char& c:s) if(c>='A' && c<='Z') c+=32; return s; }
template<class F> void rows(std::string_view data,F fn) {
    std::istringstream stream{std::string(data)}; std::string row;
    while(std::getline(stream,row)) { if(!row.empty() && row.back()=='\r') row.pop_back(); auto p=row.find('\t'); if(p!=std::string::npos) fn(row.substr(0,p),std::stod(row.substr(p+1))); }
}
}
Composition compose(std::string_view keys) {
    Composition out; wchar_t c=0,v=0,f=0; size_t start=0;
    auto flush=[&](size_t end) {
        if(c && v) { out.text.push_back(static_cast<wchar_t>(0xac00+(initials.find(c)*21+vowels.find(v))*28+(f?finals.find(f):0))); out.boundaries.push_back(end); }
        else if(c || v) { out.text.push_back(c?c:v); out.boundaries.push_back(end); }
        c=v=f=0; start=end;
    };
    for(size_t i=0;i<keys.size();++i) {
        wchar_t j=jamo(keys[i]);
        if(!j) { flush(i); out.text.push_back(static_cast<unsigned char>(keys[i])); out.boundaries.push_back(i+1); start=i+1; continue; }
        if(vowels.find(j)!=std::wstring_view::npos) {
            if(v) {
                if(f) {
                    wchar_t move=f,stay=0; for(auto p:finalPairs) if(p.c==f) { stay=p.a; move=p.b; }
                    f=stay; flush(i-1); c=move; v=j;
                } else if(auto joined=combine(v,j,vowelPairs)) v=joined;
                else { flush(i); v=j; }
            } else v=j;
        } else {
            if(!v || !c) { if(c || v) flush(i); c=j; }
            else if(f) { if(auto joined=combine(f,j,finalPairs)) f=joined; else { flush(i); c=j; } }
            else if(finals.find(j)!=std::wstring_view::npos) f=j;
            else { flush(i); c=j; }
        }
    }
    (void)start;
    flush(keys.size()); return out;
}
Engine::Engine(std::string_view english,std::string_view korean,std::string_view syllables) {
    rows(english,[&](const std::string& word,double level) { english_[word]=static_cast<int>(level); });
    std::istringstream extras("hello github api json yaml xml html css windows microsoft chrome edge atypoi git commit push pull merge branch repo vscode slack teams kakao url http https www email app apps cpu gpu ui ux ok lol omg btw thx pls python javascript typescript rust cpp");
    std::string word; while(extras>>word) english_[word]=20;
    for(const auto& entry:english_) {
        int a=0,b=0;
        for(size_t i=0;i<=entry.first.size();++i) {
            char c=i<entry.first.size()?entry.first[i]:0;
            if(c && (c<'a'||c>'z')) continue;
            int d=c?c-'a'+1:0;
            trigrams_[(a*27+b)*27+d]++; contexts_[a*27+b]++; a=b;b=d;
        }
    }
    rows(korean,[&](const std::string& w,double) { korean_.insert(fromUtf8(w)); });
    // Supplemental product vocabulary, not a substitute for the bundled corpus.
    for(auto w:{L"안녕",L"안녕하세요",L"한글",L"한국어",L"감사합니다",L"윈도우",L"입력기"}) korean_.insert(w);
    double total=0; rows(syllables,[&](const std::string&,double count) { total+=count; });
    rows(syllables,[&](const std::string& w,double count) { auto s=fromUtf8(w); if(s.size()==1) syllables_[s[0]]=std::log((count+0.5)/(total+5586)); });
    const std::wstring common=L"이다는의에가하고지을를은기로서사한도니시대리자아스어인정나수일해있게들구전적주상국요부과제만라원장보동거그생내여것면했성소중오모히마경각무공위신데화문연계같실습때조개비회세미저진할업되된말야안까네없좋우너";
    for(auto c:common) { auto it=syllables_.find(c); syllables_[c]=it==syllables_.end()?-5.0:std::max(it->second,-5.0); }
}
bool Engine::knownEnglish(std::string_view text) const {
    auto word=lower(text); auto it=english_.find(word); if(it!=english_.end() && it->second<=50) return true;
    for(std::string suffix:{"s","es","ed","ing","ly","er","est"}) if(word.size()>suffix.size()+2 && word.ends_with(suffix)) {
        auto base=word.substr(0,word.size()-suffix.size()); if(english_.contains(base)||english_.contains(base+"e")) return true;
    }
    return false;
}
double Engine::englishScore(std::string_view text) const {
    auto word=lower(text); double score=0; int a=0,b=0;
    for(size_t i=0;i<=word.size();++i) {
        int c=i<word.size()?word[i]-'a'+1:0; if(c<0||c>26) return 0;
        score+=std::log((trigrams_[(a*27+b)*27+c]+0.5)/(contexts_[a*27+b]+13.5)); a=b;b=c;
    }
    score/=static_cast<double>(word.size()+1);
    if(knownEnglish(text)) score=std::max(score,word.size()>=4?-1.4:word.size()==3?-2.0:-1.9);
    return score;
}
std::optional<std::wstring> Engine::mixed(std::string_view keys) const {
    const std::vector<std::wstring> particles={L"은",L"는",L"이",L"가",L"을",L"를",L"에",L"에서",L"의",L"도",L"로",L"으로"};
    if(knownEnglish(keys)) return {};
    for(size_t split=keys.size();split-->2;) {
        auto prefix=keys.substr(0,split); bool acronym=prefix.size()>=2 && std::all_of(prefix.begin(),prefix.end(),[](char c){return c>='A'&&c<='Z';});
        if(!acronym && !knownEnglish(prefix)) continue;
        auto suffix=compose(keys.substr(split)).text;
        if(std::find(particles.begin(),particles.end(),suffix)!=particles.end()) return fromUtf8(prefix)+suffix;
    }
    return {};
}
std::vector<Decision> Engine::candidates(std::string_view keys) const {
    std::vector<Decision> out={{Language::English,fromUtf8(keys),0}};
    if(auto m=mixed(keys)) out.push_back({Language::Mixed,*m,0});
    if(!keys.empty() && std::all_of(keys.begin(),keys.end(),[](char c){return jamo(c)!=0;})) out.push_back({Language::Korean,compose(keys).text,0});
    return out;
}
Decision Engine::decide(std::string_view keys,std::optional<Language> previous,bool strict) const {
    Decision e{Language::English,fromUtf8(keys),0}; if(keys.empty()) return e;
    if(!std::all_of(keys.begin(),keys.end(),[](char c){return jamo(c)!=0;})) return e;
    if(auto m=mixed(keys)) return {Language::Mixed,*m,2.0};
    // Shift held after ㄲ/ㅆ also affects ordinary Latin keys (e.g. R+N for 뀌).
    // Those keys have the same Korean jamo; preserve the original case for English.
    if(!koreanCase(keys)||(hasOrdinaryCapital(keys)&&(knownEnglish(keys)||std::all_of(keys.begin(),keys.end(),[](char c){return c>='A'&&c<='Z';}))))return e;
    auto composed=compose(keys).text; double k=0; size_t stray=0;
    bool chat=composed.size()>=2 && std::all_of(composed.begin(),composed.end(),[](wchar_t c){return std::wstring_view(L"ㅋㅎㅠㅜㄷㅇㄱㅅㅂㄴ").find(c)!=std::wstring_view::npos;});
    for(auto c:composed) { if(isSyllable(c)) { auto it=syllables_.find(c); k+=it==syllables_.end()?-11:it->second; } else { k+=chat?-4:-13; ++stray; } }
    k/=static_cast<double>(keys.size()); bool knownK=korean_.contains(composed);
    if(knownK) k=std::max(k,composed.size()>=2?-1.4:-2.0);
    double en=englishScore(keys); bool knownE=knownEnglish(keys);
    // A shifted final remains evidence after a following Korean ending is typed.
    // Keep dictionary English casing protected, but don't bias an unknown token
    // back to English just because its ㄲ/ㅆ is now inside the completed word.
    bool shiftedFinal=(keys.back()=='R'||keys.back()=='T')&&!composed.empty()&&isSyllable(composed.back());
    bool shiftedKorean=!stray&&!knownE&&keys.find_first_of("QWERTOP")!=std::string_view::npos;
    if(previous) { if(*previous==Language::English) en+=(shiftedFinal||shiftedKorean)?0.0:knownK?0.2:0.8; else k+=knownE?0.2:0.8; }
    double gap=k-en; double threshold=strict?1.4:0.15;
    if(chat && !knownE && !strict) return {Language::Korean,composed,2.0};
    if(!stray && gap>threshold) return {Language::Korean,composed,gap};
    e.margin=-gap; return e;
}
void Session::refresh(Mode mode,bool strict) {
    if(mode==Mode::Korean) display_={Language::Korean,compose(keys_).text,0};
    else if(mode==Mode::English||mode==Mode::Paused) display_={Language::English,fromUtf8(keys_),0};
    else if(forced_) {
        auto options=engine_->candidates(keys_); auto it=std::find_if(options.begin(),options.end(),[&](auto& d){return d.language==*forced_;});
        display_=it!=options.end()?*it:options.front();
    } else display_=engine_->decide(keys_,previous_,strict);
}
bool Session::append(char c,Mode mode,bool strict) {
    if(keys_.size()>=maxKeys) return false;
    bool wasKorean=display_.language==Language::Korean;
    keys_.push_back(c); deletingEarlierSyllable_=false; refresh(mode,strict);
    bool koreanKey=koreanCase(keys_);
    if(mode==Mode::Auto&&!forced_&&wasKorean&&display_.language==Language::English&&koreanKey&&!engine_->knownEnglish(keys_)) {
        auto text=compose(keys_).text;
        // An established Korean word may end with the next syllable's initial.
        // Re-evaluate when its vowel arrives so an English word can still recover.
        if(text.size()>1&&initials.find(text.back())!=std::wstring_view::npos&&std::all_of(text.begin(),text.end()-1,isSyllable))display_={Language::Korean,std::move(text),0};
    }
    return true;
}
void Session::backspace(Mode mode,bool strict) {
    if(keys_.empty()) return;
    bool wasKorean=display_.language==Language::Korean;
    if(display_.language==Language::Korean && deletingEarlierSyllable_) {
        auto c=compose(keys_); keys_.resize(c.boundaries.size()>1?c.boundaries[c.boundaries.size()-2]:0);
    } else {
        auto before=compose(keys_).text; keys_.pop_back();
        if(display_.language==Language::Korean && compose(keys_).text.size()<before.size()) deletingEarlierSyllable_=true;
    }
    refresh(mode,strict);
    // Deleting Korean composition must not turn its remaining jamo into ASCII.
    if(mode==Mode::Auto&&!forced_&&wasKorean&&!keys_.empty())display_={Language::Korean,compose(keys_).text,0};
}
void Session::cycle() {
    if(keys_.empty()) return;
    auto options=engine_->candidates(keys_); size_t pos=0;
    for(size_t i=0;i<options.size();++i) if(options[i].language==display_.language) pos=i;
    display_=options[(pos+1)%options.size()]; forced_=display_.language;
}
std::wstring Session::commit() { auto text=display_.text; if(!keys_.empty()) previous_=display_.language; keys_.clear(); forced_.reset(); display_={}; deletingEarlierSyllable_=false; return text; }
void Session::reset() { commit(); previous_.reset(); }
}
