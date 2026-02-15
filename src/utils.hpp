#pragma once
#include <Geode/binding/TextArea.hpp>
#include <Geode/binding/MultilineBitmapFont.hpp>
#include <Geode/loader/Mod.hpp>
#include "animated-sprite.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>

inline cocos2d::ccColor3B getTextAreaColor(TextArea const* textArea) {
    auto lines = textArea->m_label->m_lines;
    if (!lines || lines->count() == 0) {
        return cocos2d::ccc3(255, 255, 255);
    }

    auto lineChars = static_cast<cocos2d::CCLabelBMFont*>(lines->objectAtIndex(0))->getChildren();
    if (!lineChars || lineChars->count() == 0) {
        return cocos2d::ccc3(255, 255, 255);
    }

    return static_cast<cocos2d::CCSprite*>(lineChars->objectAtIndex(0))->getColor();
}

inline void findAndReplace(std::string& str, std::string_view find, std::string_view replace) {
    size_t pos = 0;
    while ((pos = str.find(find, pos)) != std::string::npos) {
        str.replace(pos, find.length(), replace);
        pos += replace.length();
    }
}

template <size_t N>
struct StringLiteral {
    static constexpr size_t Size = N;
    char value[N]{};
    constexpr StringLiteral() = default;
    constexpr StringLiteral(char const (&str)[N]) { std::copy_n(str, N, value); }
    constexpr operator std::string_view() const { return { value, N - 1 }; }
    constexpr operator geode::geode_internal::StringConcatModIDSlash<N>() const { return value; }
};

template <size_t N>
struct StringLiteralUTF32 {
    static constexpr size_t Size = N;
    char32_t value[N]{};
    constexpr StringLiteralUTF32(char32_t const (&str)[N]) { std::copy_n(str, N, value); }
    constexpr StringLiteralUTF32(char32_t chr) { value[0] = chr; }
    constexpr operator std::u32string_view() const { return { value, N - 1 }; }
};

template <size_t N>
struct EmojiToHexConverter {
    static constexpr size_t length = N - 1;
    static constexpr size_t modIdSize = sizeof(GEODE_MOD_ID) - 1;

    char32_t value[length]{};
    char filename[modIdSize + (length * 6 + 4) + 1]{};

    constexpr EmojiToHexConverter(char32_t const (&str)[N]) {
        std::copy_n(str, N - 1, value);

        constexpr char hex[] = "0123456789abcdef";
        size_t index = 0;
        // add the mod id
        for (size_t i = 0; i < modIdSize; ++i) {
            filename[index++] = GEODE_MOD_ID[i];
        }
        filename[index++] = '/';

        // expand the emoji to hex
        for (size_t i = 0; i < length; ++i) {
            int c = str[i];

            // if last char is 0xfe0f, skip it
            if (c == 0xfe0f) {
                // if this was a last char, remove the dash
                if (filename[index - 1] == '-' && i == length - 1) {
                    index--;
                }
                continue;
            }

            if (c >= 0x10000) filename[index++] = hex[(c >> 16) & 0xF];
            if (c >= 0x1000) filename[index++] = hex[(c >> 12) & 0xF];
            if (c >= 0x100) filename[index++] = hex[(c >> 8) & 0xF];
            if (c >= 0x10) filename[index++] = hex[(c >> 4) & 0xF];
            filename[index++] = hex[c & 0xF];
            if (i < length - 1) filename[index++] = '-';
        }

        // add the extension
        filename[index++] = '.';
        filename[index++] = 'p';
        filename[index++] = 'n';
        filename[index++] = 'g';
        filename[index] = '\0';
    }

    constexpr std::pair<std::u32string_view, char const*> operator()() const {
        return { std::u32string_view(value, length), filename };
    }
};

template <EmojiToHexConverter S>
constexpr std::pair<std::u32string_view, char const*> operator""_emoji() { return S(); }

template <char32_t C>
constexpr auto SingleEmoji = EmojiToHexConverter(StringLiteralUTF32<2>{C}.value);

template <char32_t C>
constexpr std::pair<std::u32string_view, char const*> Sprite = {
    std::u32string_view(SingleEmoji<C>.value, SingleEmoji<C>.length),
    SingleEmoji<C>.filename
};

struct Emoji {
    std::string_view name;
    std::string_view emoji;

    constexpr Emoji() = default;
    constexpr Emoji(std::string_view name, std::string_view emoji) : name(name), emoji(emoji) {}
};

template <char32_t C>
struct UTF32ToUTF8 {
    // static_assert(C >= 0x1C000 && C <= 0x1CFFF, "Custom emojis must be in the range 0x1C000-0x1CFFF");
    static constexpr char32_t value = C;
    static constexpr std::array<char, 5> utf8 = {
        static_cast<char>(0xF0 | C >> 18),
        static_cast<char>(0x80 | ((C >> 12) & 0x3F)),
        static_cast<char>(0x80 | ((C >> 6) & 0x3F)),
        static_cast<char>(0x80 | (C & 0x3F)),
        '\0'
    };
    static constexpr std::string_view utf8_view = { utf8.data(), utf8.size() - 1 };
};

template <StringLiteral Name, char32_t C>
static constexpr auto CustomEmoji = Emoji{ Name, UTF32ToUTF8<C>::utf8_view };

template <StringLiteral Name, char32_t C>
struct custom_emoji {
    static constexpr auto emoji = CustomEmoji<Name, C>;
    static constexpr auto sprite = Sprite<C>;
    static constexpr char32_t value = C;

    constexpr operator Label::EmojiMap::value_type() const { return sprite; }
    constexpr operator Emoji() const { return emoji; }
};

template <geode::geode_internal::StringConcatModIDSlash Prefix>
cocos2d::CCNode* GetFrameAnimation(std::u32string_view, uint32_t&) {
    geode::log::debug("Creating {}", Prefix.buffer);
    return cocos2d::CCSprite::create(Prefix.buffer);
}

template <StringLiteral Name, bool Animated, char32_t C>
struct animoji {
    static constexpr auto Name2 = []{
        StringLiteral<Name.Size + 2> name;
        name.value[0] = ':';
        std::copy_n(Name.value, Name.Size, name.value + 1);
        name.value[Name.Size] = ':';
        return name;
    }();
    static constexpr auto FileName = [] {
        StringLiteral<Name.Size + 5> name;
        std::copy_n(Name.value, Name.Size, name.value);
        name.value[Name.Size - 1] = '.';
        name.value[Name.Size + 0] = 'w';
        name.value[Name.Size + 1] = 'e';
        name.value[Name.Size + 2] = 'b';
        name.value[Name.Size + 3] = 'p';
        return name;
    }();
    static constexpr auto emoji = CustomEmoji<Name2, C>;
    static constexpr geode::geode_internal::StringConcatModIDSlash<FileName.Size> prefix = FileName;
    static constexpr char32_t value = C;
    static constexpr auto name = std::u32string_view(SingleEmoji<C>.value, SingleEmoji<C>.length);
    static constexpr auto func = &GetFrameAnimation<prefix>;

    constexpr operator Emoji() const { return emoji; }
    constexpr operator Label::CustomNodeMap::value_type() const {
        return { name, func };
    }
};

template <StringLiteral Name, StringLiteral Utf8, StringLiteralUTF32 Utf32, bool Animated = false, bool Hidden = false>
struct UnimojiBase {
    static constexpr auto name = Name;
    static constexpr auto placeholder = [] {
        StringLiteral<Name.Size + 2> name;
        name.value[0] = ':';
        std::copy_n(Name.value, Name.Size, name.value + 1);
        name.value[Name.Size] = ':';
        name.value[Name.Size + 1] = '\0';
        return name;
    }();
    static constexpr auto utf8 = Utf8;
    static constexpr auto utf32 = Utf32;
    static constexpr auto isHidden = Hidden;
    static constexpr auto isAnimated = Animated;

    static constexpr auto converter = EmojiToHexConverter<Utf32.Size>{ Utf32.value };
    static constexpr auto sprite = std::pair<std::u32string_view, char const*> { Utf32.value, converter.filename };
    static constexpr auto emoji = Emoji{ placeholder, Utf8 };
    static constexpr auto animatedSprite = animoji<Name, Animated, Utf32.value[0]>{};
};

template <StringLiteral Name, char32_t C, bool Animated = false, bool Hidden = false>
struct Unimoji : UnimojiBase<Name, StringLiteral<1>{}, StringLiteralUTF32<2>{C}, Animated, Hidden> {
    static constexpr auto emojiChar = C;
    static constexpr auto emoji = CustomEmoji<Unimoji::placeholder, C>;
    static constexpr auto sprite = Sprite<C>;
    static constexpr auto animatedSprite = animoji<Name, Animated, C>{};
};

template <StringLiteral Name, char32_t C>
using HiddenUnimoji = Unimoji<Name, C, false, true>;

template <StringLiteral Name, StringLiteral Utf8, StringLiteralUTF32 Utf32, bool Hidden = false>
using UnimojiUtf8 = UnimojiBase<Name, Utf8, Utf32, false, Hidden>;

struct EmojiCategory {
    std::string_view name;
    std::string_view icon;
    std::vector<std::string> emojis;
};

using EmojiMapEntry = std::pair<std::u32string_view, char const*>;
using AnimatedEntry = std::pair<std::u32string_view, cocos2d::CCNode*(*)(std::u32string_view, uint32_t&)>;

template <StringLiteral Name, StringLiteral Icon, typename... Emojis>
struct EmojiGroup {
    static constexpr auto GroupName = Name;
    static constexpr auto IconName = Icon;
    static constexpr std::tuple<Emojis...> EmojiTuple = { Emojis{}... };
    static constexpr auto TotalSize = sizeof...(Emojis);
    static constexpr auto Size = [] {
        size_t size = 0;
        (..., (size += Emojis::isHidden ? 0 : 1));
        return size;
    }();
    static constexpr auto AnimatedCount = [] {
        size_t count = 0;
        (..., (count += Emojis::isHidden || !Emojis::isAnimated ? 0 : 1));
        return count;
    }();
    static constexpr auto RegularCount = Size - AnimatedCount;
    static constexpr auto Animated = AnimatedCount > 0;

    static consteval std::array<Emoji, TotalSize> getReplacements() {
        std::array<Emoji, TotalSize> replacements;
        size_t index = 0;
        ((replacements[index++] = Emojis::emoji), ...);
        return replacements;
    }

    static consteval std::array<EmojiMapEntry, RegularCount> getRegular() {
        std::array<EmojiMapEntry, RegularCount> regular;
        size_t index = 0;
        ((Emojis::isHidden || Emojis::isAnimated ? void() : void(regular[index++] = Emojis::sprite)), ...);
        return regular;
    }

    static consteval std::array<AnimatedEntry, AnimatedCount> getAnimated() {
        std::array<AnimatedEntry, AnimatedCount> animated{};
        size_t index = 0;
        ((!Emojis::isHidden && Emojis::isAnimated ? void(animated[index++] = {
            Emojis::animatedSprite.name,
            Emojis::animatedSprite.func
        }) : void()), ...);
        return animated;
    }

    static EmojiCategory getCategory() {
        std::vector<std::string> emojis;
        ((Emojis::isHidden ? void() : emojis.push_back(std::string(Emojis::placeholder))), ...);
        return {
            Name, Icon,
            std::move(emojis)
        };
    }
};

template <size_t Index>
constexpr size_t CountEmojisImpl(auto tuple) {
    if constexpr (Index == std::tuple_size_v<decltype(tuple)>) {
        return 0;
    } else {
        return std::get<Index>(tuple).TotalSize + CountEmojisImpl<Index + 1>(tuple);
    }
}

template <typename T, size_t S>
constexpr auto array_to_tuple(std::array<T, S> const& arr) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return std::tuple(arr[I]...);
    }(std::make_index_sequence<S>{});
}

template <size_t Index = 0, size_t Size = 0>
constexpr auto CombineReplacements(auto tuple) {
    if constexpr (Index == std::tuple_size_v<decltype(tuple)>) {
        return std::array<Emoji, Size>{};
    } else {
        using Entry = std::tuple_element_t<Index, decltype(tuple)>;
        if constexpr (Entry::TotalSize == 0) {
            return CombineReplacements<Index + 1, Size>(tuple);
        } else {
            constexpr auto children = Entry::getReplacements();
            auto concat = CombineReplacements<Index + 1, Size + children.size()>(tuple);
            std::copy(children.begin(), children.end(), concat.begin() + Size);
            return concat;
        }
    }
}

template <size_t Index = 0, size_t Size = 0>
constexpr auto CombineRegulars(auto tuple) {
    if constexpr (Index == std::tuple_size_v<decltype(tuple)>) {
        return std::array<EmojiMapEntry, Size>{};
    } else {
        using Entry = std::tuple_element_t<Index, decltype(tuple)>;
        if constexpr (Entry::RegularCount == 0) {
            return CombineRegulars<Index + 1, Size>(tuple);
        } else {
            constexpr auto children = Entry::getRegular();
            auto concat = CombineRegulars<Index + 1, Size + children.size()>(tuple);
            std::copy(children.begin(), children.end(), concat.begin() + Size);
            return concat;
        }
    }
}

template <size_t Index = 0, size_t Size = 0>
constexpr auto CombineAnimated(auto tuple) {
    if constexpr (Index == std::tuple_size_v<decltype(tuple)>) {
        return std::array<AnimatedEntry, Size>{};
    } else {
        using Entry = std::tuple_element_t<Index, decltype(tuple)>;
        if constexpr (Entry::AnimatedCount == 0) {
            return CombineAnimated<Index + 1, Size>(tuple);
        } else {
            constexpr auto children = Entry::getAnimated();
            auto concat = CombineAnimated<Index + 1, Size + children.size()>(tuple);
            std::copy(children.begin(), children.end(), concat.begin() + Size);
            return concat;
        }
    }
}

template <size_t Index = 0>
void PopulateCategoryInfos(auto tuple, std::vector<EmojiCategory>& categories) {
    if constexpr (Index != std::tuple_size_v<decltype(tuple)>) {
        using Entry = std::tuple_element_t<Index, decltype(tuple)>;
        categories.push_back(std::move(Entry::getCategory()));
        PopulateCategoryInfos<Index + 1>(tuple, categories);
    }
}