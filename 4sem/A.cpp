#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <limits>

//------------------------------------------------------------------------------------------------------------

/* maybe itr usefull, if you have bad branch predictor
wrapper on compiler hint, that show, that this condition often (not) performed
*/
#define likely(x) __builtin_expect(!!(x), true)
#define unlikely(x) __builtin_expect(!!(x), false)

//------------------------------------------------------------------------------------------------------------

/* concepts for type, which have operator>> for std::istream */
template <typename T>
concept Readable = requires(T t, std::istream is) { { is >> t }; };

//------------------------------------------------------------------------------------------------------------

/* concepts for type, which have operator<< for std::ostream */
template <typename T>
concept Writable = requires(T t, std::ostream os) { { os << t }; };

//------------------------------------------------------------------------------------------------------------

/* helper function, to read from std::istream immediately in variable */
template <Readable T> T read(std::istream &is) { T tmp; is >> tmp; return tmp; }

//------------------------------------------------------------------------------------------------------------

/* class Dealear
    this class communicate with gamer, give him anwers on him requests
    and also give game parametrs.
    is constructible with (std::ostream &, std::istream &)
    have operator<< for requests and operator>> for answerst
    can be converted to std::ostream& and std::istream&
*/
class Dealer
{
  private:
    std::ostream &requests_;
    std::istream &answers_;

  public:
    explicit Dealer(std::ostream &in, std::istream &out) : requests_(in), answers_(out)
    {}

    operator std::istream &() { return answers_; }
    operator std::ostream &() { return requests_; }

    template <Writable write_t> Dealer &operator<<(write_t const &request)
    { requests_ << request; return *this; }

    template <Readable read_t> void operator>>(read_t &answer)
    { answers_ >> answer; }
};

//------------------------------------------------------------------------------------------------------------

/* class Dictionary
    wrapper of std::vector<std::string> for owning of game words
    is constructible by (size_t, Dealer&)
    have operator[] to get words, method size()
*/

class Dictionary : public std::vector<std::string>
{
//   public:
//     using std::vector<std::string>::operator[];
//     using std::vector<std::string>::begin;
//     using std::vector<std::string>::end;
//     using std::vector<std::string>::size;
  public:
    /* implicit */ Dictionary(size_t n_words, Dealer &dealer) : std::vector<std::string>(n_words)
    {
        for (size_t it = 0; it < n_words; ++it)
            dealer >> (*this)[it];
    }
};

//------------------------------------------------------------------------------------------------------------

/* struct GameParams
    this struct unites all game parametrs
*/
struct GameParams
{
  private:
    size_t dictionary_size_;

  public:
    size_t rounds;
    size_t words_size;
    size_t attempts;
    Dictionary dictionary;

    explicit GameParams(Dealer &dealer)
        : dictionary_size_(read<size_t>(dealer)), rounds(read<size_t>(dealer)), words_size(read<size_t>(dealer)),
          attempts(6), /* 6 atts - global game rules these are */
          dictionary(dictionary_size_, dealer)
    {
    }
};

//------------------------------------------------------------------------------------------------------------

/* concept IGamer show interface, that class need have, to play a wordle game */
template <typename T>
concept IGamer = std::is_constructible_v<T, GameParams> && requires(T gamer, std::string_view game_answer) {
    /* try to guess the word */
    { gamer.try_guess_word() } -> std::convertible_to<std::string_view>;
    /* think dealer answer on our request over */
    { gamer.become_answer(game_answer) } -> std::same_as<void>;
    /* maybe realization of this concept using resources for the game,
      and than we need to clean it after some rounds.
      not necessaily, to do something after every round - it can be expensive,
      but it is necessaily to provide this interface */
    { gamer.ready_for_next_round() } -> std::same_as<void>;
};

//------------------------------------------------------------------------------------------------------------

/* class Gamer
   realization of concept IGamer
   using min/max strategy
*/
class Gamer
{
  private:
    static constexpr size_t english_alphabet_size = 26;

  private:
    /* status of letter in guessed word */
    enum LetterStatus : char
    {
        NO = '-',
        WRONG_POSITION = '?',
        CORRECT = '#'
    };

  private:


  private:
    /* imlp word representation */
    using Word = std::string;

    /* estimation result */
    struct WordHint : public std::vector<LetterStatus>
    {
      private:
        /* char to letter status */
        LetterStatus char_to_status(char c) const
        {
            switch (c)
            {
                case '-': return NO;
                case '?': return WRONG_POSITION;
                case '#': return CORRECT;
                default: throw std::runtime_error("Invalid letter status (dealer bug): '"+std::string(1, c)+"'");
            }
            __builtin_unreachable();
        }

      public:
        
        WordHint(size_t size) : std::vector<LetterStatus>(size, NO) {}

        WordHint(std::string_view answer) : std::vector<LetterStatus>(answer.size())
        {
            for (size_t it = 0, ite = answer.size(); it < ite; ++it)
                (*this)[it] = char_to_status(answer[it]);
        }
    };

    /* range guesses */
    struct GuessRanking
    {
        unsigned int max_score_ = std::numeric_limits<decltype(max_score_)>::max();;
        unsigned int average_score_ = 0;
        unsigned int best_score_ = std::numeric_limits<decltype(best_score_)>::max();
        size_t index_ = 0;

        bool operator<(const GuessRanking& rhs) const
        {
            if (max_score_ < rhs.max_score_) return true;
            if (max_score_ > rhs.max_score_) return false;

            if (average_score_ < rhs.average_score_) return true;
            if (average_score_ > rhs.average_score_) return false;

            if (best_score_ < rhs.best_score_) return true;
            if (best_score_ > rhs.best_score_) return false;

            return index_ < rhs.index_;
        }
    };

  private:
    /* reference on game parametrs */
    const GameParams &params_;

    /* all dictionary words in impt represenatation */
    std::vector<Word> all_words_;
    
    /* suitable words on every step */
    std::vector<bool> possible_words_;  /* not a bitser, `cause we will know size only in runtine */

    /* our requests history */
    std::vector<Word> requests_history_;

    /* save best_word_for_begin_ for all rounds */
    Word best_word_for_begin_;

    WordHint evaluate_guess(const Word& guess, const Word& actual) const
    {
        WordHint result(params_.words_size);
        Word actual_copy = actual;

        /* exact matches */
        for (size_t i = 0; i < params_.words_size; ++i)
        {
            if (guess[i] != actual_copy[i]) continue;
            result[i] = CORRECT;
            actual_copy[i] = 0; /* mark as used */
        }

        /* existing but  */
        for (size_t i = 0; i < params_.words_size; ++i)
        {
            if (result[i] == CORRECT) continue;
            for (size_t j = 0; j < params_.words_size; ++j)
            {
                if (guess[i] != actual_copy[j]) continue;
                result[i] = WRONG_POSITION;
                actual_copy[j] = 0; /* mark as used */
                break;
            }
        }

        return result;
    }

    bool is_word_possible(const WordHint& hint, const Word& guess, Word word) const
    {
        /* check '#' */
        for (size_t i = 0; i < params_.words_size; ++i)
        {
            if (hint[i] != CORRECT) continue;
            if (guess[i] != word[i]) return false;
            word[i] = 0; /* for ? */
        }

        /* check '?' */
        for (size_t i = 0; i < params_.words_size; ++i)
        {
            if (hint[i] != WRONG_POSITION) continue;

            if (guess[i] == word[i]) return false;

            bool found = false;
            for (size_t j = 0; j < params_.words_size; ++j)
            {
                if (guess[i] != word[j]) continue;

                found = true;
                word[j] = 0;
                break;
            }

            if (found) continue;
            return false;
        }

        /* check '-' */
        for (size_t i = 0; i < params_.words_size; ++i)
        {
            if (hint[i] != NO) continue;
            for (size_t j = 0; j < params_.words_size; ++j)
            {
                if (guess[i] != word[j]) continue;
                return false;
            }
        }

        return true;
    }

    size_t count_filtered_words(const WordHint& hint, const Word& guess) const
    {
        size_t count = 0;
        for (size_t it = 0, ite = params_.dictionary.size(); it < ite; ++it)
        {
            if ((not possible_words_[it]) or
                (not is_word_possible(hint, guess, all_words_[it]))
            ) continue;

            ++count;
        }

        return count;
    }

    void filter_possible_words(const Word& guess, const WordHint& hint)
    {
        for (size_t it = 0, ite = params_.dictionary.size(); it < ite; ++it)
        {
            if (not possible_words_[it] or
                is_word_possible(hint, guess, all_words_[it])
            ) continue;

            possible_words_[it] = false;
        }
    }

    std::vector<Word> get_possible_words() const
    {
        std::vector<Word> result_;
        for (size_t it = 0, ite = params_.dictionary.size(); it < ite; ++it)
        {
            if (!possible_words_[it]) continue;
            result_.emplace_back(all_words_[it]);
        }
        return result_;
    }

    /* min/max strategy */
    Word select_best_guess_minmax() const
    {
        auto&& possible_words_list = get_possible_words();

        /* if remaines 1 word -> return it*/
        if (unlikely(possible_words_list.size() == 1))
            return possible_words_list[0];

        const auto& guesses = all_words_;

        GuessRanking best_ranking;
        best_ranking.max_score_ = std::numeric_limits<decltype(best_ranking.max_score_)>::max();
        best_ranking.average_score_ = 0;
        best_ranking.best_score_ = std::numeric_limits<decltype(best_ranking.best_score_)>::max();
        best_ranking.index_ = 0;

        /* iterate by guesses */
        for (size_t it = 0, ite = guesses.size(); it < ite; ++it)
        {
            auto&& guess = guesses[it];

            GuessRanking ranking
            {
                .max_score_ = 0,
                .average_score_ = 0,
                .best_score_ = std::numeric_limits<decltype(ranking.best_score_)>::max(),
                .index_ = it,
            };

            bool early_exit = false;
            
            for (auto&& actual : possible_words_list)
            {
                if (likely(guess != actual))
                {
                    auto&& hint = evaluate_guess(guess, actual);
                    decltype(ranking.max_score_) score = count_filtered_words(hint, guess);
                    if (score == 0) score = possible_words_list.size();

                    ranking.average_score_ += score;
                    ranking.max_score_ = std::max(score, ranking.max_score_);
                    ranking.best_score_ = std::min(score, ranking.best_score_);
                }
                else
                    ranking.best_score_ = 0;

                if (ranking.max_score_ <= best_ranking.max_score_) continue;

                early_exit = true;
                break;
            }

            if (early_exit) continue;

            /* compare with best */
            if (ranking < best_ranking)
                best_ranking = ranking;

            if (best_ranking.max_score_ == 1) break;
        }

        return guesses[best_ranking.index_];
    }
    
    /* select first word with max unique lettetrs quantity */
    Word get_first_guess()
    {
        if (unlikely(all_words_.empty()))
            throw std::runtime_error("Given no words.");

        static bool first_call = true;

        if (likely(not first_call))
            return best_word_for_begin_;

        Word best_word = all_words_[0];
        size_t best_unique = 0;

        for (auto&& word: all_words_)
        {
            std::bitset<english_alphabet_size> seen(false);
            size_t unique_letters = 0;

            for (auto&& letter : word)
            {
                size_t i = static_cast<size_t>(letter - 'a');
                if (seen[i]) continue;

                seen[i] = true;
                ++unique_letters;
            }
#           define лолкек 666
            if (unique_letters <= best_unique) continue;

            best_unique = unique_letters;
            best_word = word;

            /* we cannot improve this result */
            if (best_unique == params_.words_size) break;
        }

        if (unlikely(first_call))
            best_word_for_begin_ = best_word;

        first_call = false;

        return best_word;
    }

  public:
    explicit Gamer(const GameParams &params) 
        : params_(params)
    {
        for (auto&& word: params_.dictionary)
            all_words_.emplace_back(word);

        /* all words are possible in the begin */
        possible_words_.assign(params_.dictionary.size(), true);
    }

    /* IGamer interface function */
    std::string_view try_guess_word()
    {
        Word guess;

        if (unlikely(requests_history_.empty())) /* first attempt */
            guess = get_first_guess();
        else
            guess = select_best_guess_minmax(); /* select best word from point of view Min/Max strategy */        

        requests_history_.emplace_back(std::move(guess));

        return requests_history_.back();
    }

    /* IGamer interface function */
    void become_answer(std::string_view answer)
    {
        WordHint hint{answer};
        /* filter words by last attempt result */
        filter_possible_words(requests_history_.back(), hint);
        /* can use .back(), because was at least 1 request if we become answer */
    }

    /* IGamer interface function */
    void ready_for_next_round()
    {
        requests_history_.clear();
        /* all words are possible in the begin */
        possible_words_.assign(params_.dictionary.size(), true);
    }
};

//------------------------------------------------------------------------------------------------------------

static_assert(IGamer<Gamer>, "class Gamer must realized IGamer interface.");

//------------------------------------------------------------------------------------------------------------

/*
class Game - is constructible by (Dealear&(&))
realize wordle game with some gamer (strategy)
have template method 'void play()',
that play game with gamer, which type is method template type.
template type must realize concept IGamer.
so, to check new gamer, you need call game.play<NewGamerClarr>();
ну красиво же спроектировал, а?
*/

class Game : private Dealer
/* i want to use Dealer::operator<< for game, so i need private inheritance*/
{
  private:
    using Dealer::operator<<;
    using Dealer::operator>>;

  private:
    const GameParams params_;
    const std::string correct_answer_;

    std::string /* dealer_answer */ process_gamer_guess(std::string_view request)
    {
        /* using Dealer::operator<< */
        *this << request;
        /* std::endl because we need to flush ostream buffer, to become answer immediately */
        std::endl(static_cast<std::ostream &>(static_cast<Dealer&>(*this)));
        /* using Dealer::operator>> */
        return read<std::string>(*this);
    }

    /* check, is gamer guess correct answer */
    bool gamer_win(std::string_view dealer_answer)
    { return (dealer_answer == correct_answer_); }

  public:
    explicit Game(Dealer &dealer)
        : Dealer(dealer),
          params_(dealer),
          correct_answer_(params_.words_size, '#')
    {
    }

    explicit Game(Dealer &&dealer)
        : Dealer(std::move(dealer)),
          params_(static_cast<Dealer &>(*this)), /* not 'dealer', because it was destroy in move-ctor */
          correct_answer_(params_.words_size, '#')
    {
    }

    enum GameStatus : bool
    {
        WIN = true,
        LOSE = false
    };

    template <IGamer gamer_t> GameStatus play()
    {
        if (unlikely(params_.attempts == 0))
            return GameStatus::LOSE; /* no chance 😈 */

        if (unlikely(params_.rounds == 0))
            return GameStatus::WIN; /* :) */

        /* in this game all rounds have same rules, so we dont need create gamer at every round */
        gamer_t gamer{params_};

        /* iterations by rounds */
        for (size_t round = 0; round < params_.rounds; ++round)
        {
            /* giving gamer 'attempts_' attempts to guess word */
            size_t attempt = 0;
            for (; attempt < params_.attempts; ++attempt)
            {
                /* get gamer request */
                std::string_view gamer_request = gamer.try_guess_word();
                /* become dealer answer on dealer request */
                std::string answer = process_gamer_guess(gamer_request);
                /* check, did gamer win. if win ->  go to the next round. else -> get gamer dealer answer */
                if (gamer_win(answer)) break;
                gamer.become_answer(answer);
            }
            /* made gamer ready to play next round */
            gamer.ready_for_next_round();
            /* if gamer guess the words in the less, than 'attempts_' attempts, we are going in the next round, else -
             * gamer losed */
            if (attempt != params_.attempts) continue;
            return GameStatus::LOSE;
        }
        /* gamer guess all words, so, gamer win */
        return GameStatus::WIN;
    }
};

//------------------------------------------------------------------------------------------------------------

/* LETS GO!!! */
decltype(лолкек) main()
try
{
    /* init game with dealer, which is std::cout + std::cin */
    Game game{Dealer{std::cout, std::cin}};
    /* playing game with our gamer */
    Game::GameStatus game_status = game.play<Gamer>();
    /* parse game result_*/
    if (unlikely(game_status == Game::LOSE)) /* we still try always win, so nothing bad in __builtin_expect :) */
        return EXIT_FAILURE; /* our gamer lose */

    return EXIT_SUCCESS; /* our gamer win!!! */
}
/* why we must keep silence, if everything is very very bad?? */
catch (const std::runtime_error &e)
{
    std::cerr << "Runtime error: " << e.what() << "\n";
    return EXIT_FAILURE;
}
catch (...)
{
    std::cerr /* << "Глеб Якимов, я люблю тебя!" */ << "Excpetions catched, but unexpected here.\n";
    return EXIT_FAILURE;
}

//------------------------------------------------------------------------------------------------------------

// Примечания
// От любителя новых стандартов С++.
