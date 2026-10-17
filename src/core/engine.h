#pragma once
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace atypoi {
enum class Language { English, Mixed, Korean };
enum class Mode { Auto, Korean, English, Paused };
struct Composition { std::wstring text; std::vector<size_t> boundaries; };
std::wstring fromUtf8(std::string_view text);
std::string toUtf8(std::wstring_view text);
Composition compose(std::string_view keys);
struct Decision {
    Language language = Language::English;
    std::wstring text;
    double margin = 0;
};
class Engine {
    friend class Session;
public:
    Engine(std::string_view english, std::string_view korean, std::string_view syllables);
    Decision decide(std::string_view keys, std::optional<Language> previous = {}, bool strict = false) const;
    std::vector<Decision> candidates(std::string_view keys) const;
    bool ready() const { return english_.size() > 1000 && korean_.size() > 1000; }
private:
    std::unordered_map<std::string, int> english_;
    std::unordered_set<std::wstring> korean_;
    std::unordered_map<wchar_t, double> syllables_;
    std::array<double, 27 * 27 * 27> trigrams_{};
    std::array<double, 27 * 27> contexts_{};
    bool knownEnglish(std::string_view word) const;
    double englishScore(std::string_view word) const;
    std::optional<std::wstring> mixed(std::string_view keys) const;
};
class Session {
public:
    explicit Session(const Engine& engine) : engine_(&engine) {}
    bool append(char key, Mode mode, bool strict);
    void backspace(Mode mode, bool strict);
    void cycle();
    std::wstring commit();
    void reset();
    void resetContext() { previous_.reset(); }
    bool empty() const { return keys_.empty(); }
    const std::string& keys() const { return keys_; }
    const Decision& display() const { return display_; }
    static constexpr size_t maxKeys = 128;
private:
    const Engine* engine_;
    std::string keys_;
    Decision display_;
    std::optional<Language> forced_, previous_;
    bool deletingEarlierSyllable_ = false;
    void refresh(Mode mode, bool strict);
};
}
