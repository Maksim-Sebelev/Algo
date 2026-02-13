/* NEW CONTEST COMPILE FLAGS TEST */

#include <cstddef>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <sys/types.h>
#include <vector>
#include <string>
#include <string_view>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <cassert>

//------------------------------------------------------------------------------------------------------------

#define unlikely(x) __builtin_expect(!!(x), 0)

//------------------------------------------------------------------------------------------------------------

/* concepts for type, which have operator>> for std::istream */
template <typename T>
concept Readable = requires(T t, std::istream is)
{
  { is >> t };
};

//------------------------------------------------------------------------------------------------------------

/* concepts for type, which have operator<< for std::ostream */
template <typename T>
concept Writable = requires(T t, std::ostream os)
{
  { os << t };
};

//------------------------------------------------------------------------------------------------------------

/* helper function, to read from std::istream immediately in variable */
template <Readable T>
T read(std::istream& is) { T tmp; is >> tmp; return tmp; }

//------------------------------------------------------------------------------------------------------------

class Dealer
{
private:
  std::ostream& requests_;
  std::istream& answers_;

public:
  explicit Dealer(std::ostream& in, std::istream& out) :
  requests_(in), answers_(out) {}

  operator std::istream& () { return answers_; }
  operator std::ostream& () { return requests_; }

  template <Writable write_t>
  Dealer& operator << (write_t const &request) { requests_ << request; return *this; }

  template <Readable read_t>
  void operator >> (read_t &answer) { answers_ >> answer; }
};

//------------------------------------------------------------------------------------------------------------

class Dictionary
{
protected:
  /* all dictionary words */
  std::vector<std::string> words_; /* Dictionary own his words, so not a std::string_view */

public:
  size_t size() const { return words_.size(); }
  std::string_view operator[] (size_t i) const { return words_[i]; }

  /* implicit */ Dictionary(size_t n_words, Dealer& dealer) : words_(n_words)
  {
    for (size_t it = 0; it < n_words; ++it)
      dealer >> words_[it];
  }
};

//------------------------------------------------------------------------------------------------------------

/* game setting on round */
struct GameParams
{
private:
  size_t dictionary_size_;

public:
  size_t rounds;
  size_t words_size;
  size_t attempts;
  Dictionary dictionary;

  explicit GameParams(Dealer& dealer) :
    dictionary_size_(read<size_t>(dealer)),
    rounds(read<size_t>(dealer)),
    words_size(read<size_t>(dealer)),
    attempts(6), /* 6 atts - global game rules these are */
    dictionary(dictionary_size_, dealer)
  {
  }
};

//------------------------------------------------------------------------------------------------------------

/* Follow DIP with static polymophism */
template <typename T>
concept IGamer = std::is_constructible_v<T, GameParams> &&
requires(T gamer, std::string_view game_answer)
{
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

class Gamer
{
private:
  enum LetterStatus : char
  {
    NO = '-', WRONG_POSITION = '?', CORRECT = '#'
  };
 
private:
  /* no ownership, because we aare living in class Game */
  const GameParams& params_;

  std::vector<std::string> requests_history_;
  std::vector<std::vector<LetterStatus>> answers_history_;

private:

  LetterStatus get_letter_status(char c)
  {
    switch (c)
    {
      case LetterStatus::NO: return LetterStatus::NO;
      case LetterStatus::WRONG_POSITION: return LetterStatus::WRONG_POSITION;
      case LetterStatus::CORRECT: return LetterStatus::CORRECT;
      default: throw std::runtime_error("try to parse invalid letter status: '" + c + std::string("'"));
    }
    __builtin_unreachable();
  }

  std::vector<LetterStatus> parse_game_answer(std::string_view request, std::string_view game_answer)
  {
    std::vector<LetterStatus> parsed_answer(params_.words_size);
    for (size_t it = 0, ite = params_.words_size; it != ite; ++it)
      parsed_answer[it] = get_letter_status(game_answer[it]);

    return parsed_answer;
  }

public:

  explicit Gamer(const GameParams& params) : params_(params) {}

  /* try to guess the word, IGamer concept realization */
  std::string_view try_guess_word()
  {
    static size_t i = 0;

    std::string word{params_.dictionary[i++]};
  
    requests_history_.emplace_back(std::move(word));
    return requests_history_.back(); /**/
  };

  /* think dealer answer on our request over, IGamer concept realization */
  void become_answer(std::string_view answer)
  {
    answers_history_.emplace_back(parse_game_answer(requests_history_.back(), answer));

  };

  /* clear data after regular round, if need, IGamer concept realization */
  void ready_for_next_round()
  {
    static constexpr const size_t clear_round = 10; /* i want to clear gamer data after every 10 rounds */

    static size_t rounds_counter = 0;
    ++rounds_counter;

    if (rounds_counter < clear_round) return; /* return if we played less than 10 rounds after last clear/inintializatoin */

    /* clear data after 10 rounds */
    requests_history_.clear();
    answers_history_.clear();

    /* now we wait next 10 rounds before clear */
    rounds_counter = 0;
  }
};

//------------------------------------------------------------------------------------------------------------

static_assert(IGamer<Gamer>, "class Gamer must realized IGamer interface.");

//------------------------------------------------------------------------------------------------------------

class Game : private Dealer
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
    std::endl(static_cast<std::ostream&>(static_cast<Dealer>(*this)));
    /* using Dealer::operator>> */
    return read<std::string>(*this);
  }

  /* check, is gamer guess correct answer */
  bool gamer_win(std::string_view dealer_answer)
  {
    return (dealer_answer == correct_answer_);
  }

public:
  explicit Game(Dealer& dealer) :
    Dealer(dealer),
    params_(dealer),
    correct_answer_(params_.words_size, '#')
  {
  }

  explicit Game(Dealer&& dealer) :
    Dealer(dealer), /* dealer after other fields, because move-ctor destroy 'dealer' variable */
    params_(static_cast<Dealer&>(*this)), /* not 'dealer', because it was destroy in move-ctor */
    correct_answer_(params_.words_size, '#')
  {
  }

  enum GameStatus : bool { WIN = true, LOSE = false };

  template <IGamer gamer_t>
  GameStatus play()
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
      /* if gamer guess the words in the less, than 'attempts_' attempts, we are going in the next round, else - gamer losed */
      if (attempt != params_.attempts) continue;
      return GameStatus::LOSE;
    }
    /* gamer guess all words, so, gamer win */
    return GameStatus::WIN;
  }
};

//------------------------------------------------------------------------------------------------------------

/* LETS GO!!! */
auto main() -> int try
{
  /* init game with dealer, which is std::cout + std::cin */
  Game game{Dealer{std::cout, std::cin}};
  /* playing game with our gamer */
  Game::GameStatus game_status = game.play<Gamer>();
  /* parse game result*/
  if (unlikely(game_status == Game::LOSE)) /* we still try always win, so nothing bad in __builtin_expect :) */
    /* our gamer lose */
    return EXIT_FAILURE;
  /* our gamer win!!! */
  return EXIT_SUCCESS;
}
/* why we must keep silence, if everything is very very bad?? */
catch (const std::runtime_error& e)
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
