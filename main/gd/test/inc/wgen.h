#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <experimental/random>
#include <algorithm>
#include <locale>
#include <ranges>
#include <filesystem>

/// @brief  Adaptation from syllabary  https://github.com/arapelle/arba-wgen/blob/master/include/arba/wgen/syllabary.hpp
/// Package doesn't work trivially with CPM for installation, fast just to have the code locally than adapt it.

namespace wgen 
{
  class syllabary
  {
  private:
    size_t maxDirectoryDepth_;
    size_t maxFilenameLength_;
  public:
    inline static constexpr char default_format_consonant = 'c';
    inline static constexpr char default_format_vowel = 'v';
    inline static constexpr char default_format_coda = 'q';
  
    enum class Format : uint32_t
    {
      No_format, // No format
      Name,      // Name
      Lower,     // lowercase
      Upper,     // UPPERCASE
    };
  
    syllabary(const std::string_view& consonants, const std::string_view& vowels, const std::string_view& codas = "");
    syllabary(std::vector<char> consonants, std::vector<char> vowels, std::vector<char> codas = {}, size_t dirDepth = 4, size_t filenameLength = 2);
  
    std::string random_word(int max_letters, Format format = Format::No_format) const noexcept;
    std::string random_sentence(int max_words) const noexcept;
    std::string random_paragraph(int max_num_sentences) const noexcept;
    std::string random_content(int num_paragraphs = 10) const noexcept;
    std::filesystem::path random_filename(int depth = 10) const noexcept;
    std::filesystem::path random_unique_filename() const noexcept;

    inline std::string random_name(unsigned word_length) const { return random_word(word_length, Format::Name); }
    inline std::string random_lowercase_word(unsigned word_length) const { return random_word(word_length, Format::Lower); }
    inline std::string random_uppercase_word(unsigned word_length) const { return random_word(word_length, Format::Upper); }
  
    std::string format_word(std::string_view format_str,
      const char c_char = default_format_consonant,
      const char v_char = default_format_vowel,
      const char q_char = default_format_coda,
      Format format = Format::No_format) const;

    inline std::string format_name(std::string_view format_str,
      const char c_char = default_format_consonant,
      const char v_char = default_format_vowel,
      const char q_char = default_format_coda) const;

    inline std::string format_lowercase_word(std::string_view format_str,
      const char c_char = default_format_consonant,
      const char v_char = default_format_vowel,
      const char q_char = default_format_coda) const;

    inline std::string format_uppercase_word(std::string_view format_str,
      const char c_char = default_format_consonant,
      const char v_char = default_format_vowel,
      const char q_char = default_format_coda) const;
  
    std::size_t number_of_possible_words(unsigned word_length) const;
  
    inline const std::vector<char>& consonants() const { return consonants_; }
    inline const std::vector<char>& vowels() const { return vowels_; }
    inline const std::vector<char>& codas() const { return codas_; }
  
  private:
      void random_word_(char* first, char* last, unsigned word_length, Format format) const;
      void format_word_(char* first, char* last, std::string_view format_str,
                        const char c_char, const char v_char, const char q_char, Format format) const;
      void format_(char* first, char* last, Format format) const;
      char random_consonant_() const;
      char random_vowel_() const;
      char random_coda_() const;
  
      std::vector<char> consonants_;
      std::vector<char> vowels_;
      std::vector<char> codas_;
  };


  /**
   * @brief The default_syllabary class
   *
   * consonants = {'b', 'd', 'f', 'g', 'k', 'l', 'm', 'n', 'p', 'r', 't', 'v', 'y', 'z'}
   * vowels     = {'a', 'e', 'i', 'o', 'u'}
   * codas      = {'k', 'l', 'r', 'x'}
   */
  class default_syllabary : public syllabary
  {
  public:
      default_syllabary(size_t dirDepth = 4, size_t filenameLength = 2);
  };
  
  
  inline std::string syllabary::format_name(std::string_view format_str, const char c_char,
    const char v_char, const char q_char) const
  {
    return format_word(format_str, c_char, v_char, q_char, Format::Name);
  }
  
  inline std::string syllabary::format_lowercase_word(std::string_view format_str, const char c_char,
    const char v_char, const char q_char) const
  {
    return format_word(format_str, c_char, v_char, q_char, Format::Lower);
  }
  
  inline std::string syllabary::format_uppercase_word(std::string_view format_str, const char c_char,
    const char v_char, const char q_char) const
  {
    return format_word(format_str, c_char, v_char, q_char, Format::Upper);
  }
  

  syllabary::syllabary(const std::string_view &consonants, const std::string_view &vowels, const std::string_view &codas)
  : consonants_(consonants.begin(), consonants.end()),
        vowels_(vowels.begin(), vowels.end()), codas_(codas.begin(), codas.end())
  { }

  syllabary::syllabary(std::vector<char> consonants, std::vector<char> vowels, std::vector<char> codas, size_t dirDepth, size_t filenameLength)
  : consonants_(std::move(consonants)), vowels_(std::move(vowels)), codas_(std::move(codas)), maxDirectoryDepth_(dirDepth), maxFilenameLength_(filenameLength)
  { }

  std::string syllabary::random_word(int max_letters, Format format) const noexcept {
    auto word_length = std::experimental::randint(2, max_letters);
    std::string word;
    word.resize(word_length);
    random_word_(&*word.begin(), &*word.end(), word_length, format);
    return word;
  }

  // A sentence is a collection of words that start with a name and ends with a point 
  std::string syllabary::random_sentence(int max_words) const noexcept {
    auto sentence = random_word(10, Format::Name);
    auto num_words = std::experimental::randint(2, max_words);
    for (int _ : std::views::iota(0, num_words)) {
      sentence += " " + random_word(15);
    }
    sentence += ". ";
    return sentence;
  }

  // A Paragraph is a collection of sentences 
  std::string syllabary::random_paragraph(int max_sentences = 10) const noexcept {
    auto num_sentences = std::experimental::randint(2, max_sentences);

    /// Ranges at least in c++20 is a disappointment, there is no way to do map -> join, a basic use case.
    std::string paragraph;
    for (auto _: std::views::iota(0, num_sentences)) {
      paragraph += random_sentence(10);
    }
    return paragraph + "\n\n";
  }

  std::string syllabary::random_content(int num_paragraphs) const noexcept {

    std::string content;
    for (size_t i=0; i < num_paragraphs; ++i) {
      content += random_paragraph(7);
    }
    return content;
  }
  
  std::filesystem::path syllabary::random_filename(int depth) const noexcept {
    static std::vector<std::string> extensions = { "json", "txt", "md", "doc", "xls", "cpp", "rs", "py", "rb", "hs", "sh"};
    static int maxExt = extensions.size() - 1;
    auto dirs = std::experimental::randint(1, depth);

    std::filesystem::path filename;
    for (auto _: std::views::iota(0, dirs)) {
      filename = filename / random_word(maxFilenameLength_);   /// TODO: Make a decent size 
    }
    return filename.relative_path().replace_extension(extensions[std::experimental::randint(0, maxExt)]);
  }

  std::filesystem::path syllabary::random_unique_filename() const noexcept {
    static std::mutex uniqueCollectorSection;
    static std::set<std::filesystem::path> filenames;

    auto getNewFilename = [&](int depth) {
      auto filename = random_filename(depth);

      std::unique_lock<std::mutex> lock(uniqueCollectorSection);
      return filenames.insert(filename);
    };

    while (true) {
      if (auto [itr, inserted] = getNewFilename(maxDirectoryDepth_); inserted) 
        return *itr;
    }
  }

  std::string syllabary::format_word(std::string_view format_str, const char c_char, const char v_char,
    const char q_char, Format format) const
  {
    std::string word(format_str.length(), '?');
    format_word_(&*word.begin(), &*word.end(), format_str, c_char, v_char, q_char, format);
    return word;
  }

  std::size_t syllabary::number_of_possible_words(unsigned word_length) const
  {
    if (word_length > 0)
    {
        std::size_t nos2 = consonants_.size() * vowels_.size();
        std::size_t result = 1;
        for (std::size_t number_of_syllables = word_length / 2; number_of_syllables > 0; --number_of_syllables)
            result *= nos2;
        if (word_length & 1)
            result *= codas_.size() + vowels_.size();
        return result;
    }
    return 0;
  }

  void syllabary::random_word_(char* first, char* last, unsigned word_length, Format format) const
  {
    if (word_length > 0)
    {
      char* iter = first;
  
      if ((word_length & 1) && (codas_.empty() || std::experimental::randint(0,1) == 1))
      {
          *iter++ = random_vowel_();
      }
      for (std::size_t i = 0, end_i = word_length / 2; i < end_i; ++i)
      {
          *iter++ = random_consonant_();
          *iter++ = random_vowel_();
      }
      if (iter != last) 
      {
          *iter++ = codas_.empty() ? random_consonant_() : random_coda_();
      }
      format_(first, last, format);
    }
  }

  void syllabary::format_word_(char* first, char* last, std::string_view format_str,
    const char c_char, const char v_char, const char q_char, Format format) const
  {
    if (format_str.length() > 0)
    {
        char* iter = first;
        auto fmt_iter = format_str.begin();
        for (auto fmt_end_iter = codas_.empty() ? format_str.end() : format_str.end() - 1;
          fmt_iter < fmt_end_iter;
          ++fmt_iter)
        {
          char ch = *fmt_iter;
          if (ch == c_char)
              *iter = random_consonant_();
          else if (ch == v_char)
              *iter = random_vowel_();
          else if (ch == q_char)
              *iter = random_coda_();
          else
              *iter = ch;
          ++iter;
        }
        if (fmt_iter != format_str.end())
        {
          char ch = *fmt_iter;
          if (ch == c_char)
              *iter = random_consonant_();
          else if (ch == v_char)
              *iter = random_vowel_();
          else if (ch == q_char)
              *iter = random_coda_();
          else
              *iter = ch;
        }
  
        format_(first, last, format);
    }
  }

  void syllabary::format_(char* first, char* last, Format format) const
  {
    auto& facet = std::use_facet<std::ctype<std::string::value_type>>(std::locale());
    switch (format)
    {
      case Format::Name:
      {
        char& first_ch = *first;
        first_ch = facet.toupper(first_ch);
        std::for_each(std::next(first), last, [&facet](std::string::value_type& ch){ ch = facet.tolower(ch); });
        break;
      }
      case Format::Lower:
      {
        std::for_each(first, last, [&facet](std::string::value_type& ch){ ch = facet.tolower(ch); });
        break;
      }
      case Format::Upper:
      {
        std::for_each(first, last, [&facet](std::string::value_type& ch){ ch = facet.toupper(ch); });
        break;
      }
      case Format::No_format:
      default: ;
    }
  }

  char syllabary::random_consonant_() const
  {
    std::size_t idx = std::experimental::randint<std::size_t>(0, consonants_.size()-1);
    return consonants_[idx];
  }

  char syllabary::random_vowel_() const
  {
    std::size_t idx = std::experimental::randint<std::size_t>(0, vowels_.size()-1);
    return vowels_[idx];
  }

  char syllabary::random_coda_() const
  {
    std::size_t idx = std::experimental::randint<std::size_t>(0, codas_.size()-1);
    return codas_[idx];
  }

  default_syllabary::default_syllabary(size_t dirDepth, size_t filenameLength)
  : syllabary(
    {'b', 'd', 'f', 'g', 'k', 'l', 'm', 'n', 'p', 'r', 't', 'v', 'y', 'z'},
    {'a', 'e', 'i', 'o', 'u'},
    {'k', 'l', 'r', 'x'},
    dirDepth, filenameLength)
  {}
}