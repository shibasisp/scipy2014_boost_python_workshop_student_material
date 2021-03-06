// This is an implementation of rock-paper-scissors.
//
// It comprises Player base class, subclasses thereof which implement
// actual play strategies, and routines for pitting Player subclasses
// against one another.

#include <ctime>
#include <map>
#include <string>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/noncopyable.hpp>
#include <boost/python.hpp>
#include <boost/random.hpp>

namespace bp=boost::python;

// Possible moves that a player can make
enum Move {
    Rock,
    Paper,
    Scissors
};

// A Move->Move->score map.
typedef std::map<Move, std::map<Move, int> > ScoreMap;

// Returns a score-map for use in scoring rounds.
const ScoreMap& scoreMap() {
    static ScoreMap smap;
    static bool initialized = false;
    if (!initialized) {
        std::map<Move, int> rock;
        rock[Rock] = 0;
        rock[Paper] = 1;
        rock[Scissors] = -1;
        smap[Rock] = rock;

        std::map<Move, int> paper;
        paper[Rock] = -1;
        paper[Paper] = 0;
        paper[Scissors] = 1;
        smap[Paper] = paper;

        std::map<Move, int> scissors;
        scissors[Rock] = 1;
        scissors[Paper] = -1;
        scissors[Scissors] = 0;
        smap[Scissors] = scissors;

        initialized = true;
    }

    return smap;
}

/* The moves made by two players in a single round of play. */
struct Round
{
    Round(Move p1_move, Move p2_move) :
        p1(p1_move),
        p2(p2_move)
        {}

    Move p1,  // The move made by player 1
        p2;  // The move made by player 2
};

/* Compares two Moves, m1 to m2, to determine the score for the round.

   Returns -1 if m1 beats m2, 1 if m2 beats m1, and 0 for a tie.
*/
int score(Move m1, Move m2) {
    return scoreMap().find(m1)->second.find(m2)->second;
}

/* Calculate the scores for a sequence of rounds.
 */
std::vector<int> score(const std::vector<Round>& rounds) {
    std::vector<int> rslt;
    BOOST_FOREACH(const Round& r, rounds) {
        rslt.push_back(score(r.p1, r.p2));
    }
    return rslt;
}

/* The basic Player interface.

   Players have a name and implement `nextMove` for determining how
   they play.
*/
class Player
{
public:
    Player(const std::string& name) : name_(name) {}

    /* For each move a player is given the history of play up to this
     * point. The position indicates if this player is player 1
     * (my_pos=0) or player 2 (my_pos=1).
     */
    virtual Move nextMove(const std::vector<Round>& history,
                          unsigned char my_pos) const = 0;

    std::string name() const { return name_; }
    void setName(const std::string& n) { name_ = n; }

private:
    std::string name_;
};

/* Play two Players against each other for a number of rounds. Returns a sequence of scores:

   -1 -> player 1 wins
   1 -> player 2 wins
   0 -> tie
*/
std::vector<int> play(const Player& p1,
                      const Player& p2,
                      std::vector<int>::size_type num_rounds)
{
    std::vector<Round> history;
    history.reserve(num_rounds);
    for (std::vector<int>::size_type i = 0; i < num_rounds; ++i) {
        Move m1 = p1.nextMove(history, 0);
        Move m2 = p2.nextMove(history, 1);
        history.push_back(Round(m1, m2));
    }

    return score(history);
}

/* Utility class for generating random Moves.
 */
class RandomMoveGenerator
{
public:
    RandomMoveGenerator(int seed) : rng_(seed),
                                    dist_(1, 3)
        {}

    Move operator()() {
        switch (dist_(rng_)) {
            case 1:
                return Rock;
                break;

            case 2:
                return Paper;
                break;

            case 3:
            default:
                return Scissors;
                break;
        }
    }

private:
    boost::random::mt19937 rng_;
    boost::random::uniform_int_distribution<> dist_;
};

/* Generates random Moves. */
Move randomMove() {
    static RandomMoveGenerator rmg(std::time(0));
    return rmg();
}

/* A Player which simply does random moves in play. */
class Random : public Player
{
public:
    Random(const std::string& name) :
        Player(name)
        {}

    Move nextMove(const std::vector<Round>&,
                  unsigned char) const
        {
            return randomMove();
        }
};

/* A Player which simply does whatever its opponent did in the last
 * round. On the first round it plays randomly. */
class TitForTat : public Player
{
public:
    TitForTat(const std::string& name) :
        Player(name)
        {}

    Move nextMove(const std::vector<Round>& history,
                  unsigned char my_pos) const
        {
            assert(my_pos == 0 || my_pos == 1);

            if (history.empty())
                return randomMove();

            const Round& r = *history.rbegin();
            return (my_pos == 0) ? r.p2 : r.p1;
        }
};

/* Simple test which runs some rounds and prints some results. */
std::string test(std::vector<int>::size_type num_rounds=100)
{
    TitForTat p1("t4t");
    Random p2("random");
    std::vector<int> results = play(p1, p2, num_rounds);

    std::vector<int>::size_type p1_wins, p2_wins;
    p1_wins = p2_wins = 0;

    BOOST_FOREACH(int r, results) {
        if (-1 == r)
            ++p1_wins;
        else if (1 == r)
            ++p2_wins;

        std::cout << r << "\n";
    }

    if (p1_wins > p2_wins)
        return "Player " + p1.name() + " wins!";
    else if (p2_wins > p1_wins)
        return "Player " + p2.name() + " wins!";
    else
        return "It was a tie!";
}

int (*score2)(Move, Move) = &score;

BOOST_PYTHON_FUNCTION_OVERLOADS(test_overloads, test, 0, 1);

bp::list py_play(const Player& p1,
                 const Player& p2,
                 std::vector<int>::size_type num_rounds)
{
    bp::list results;

    BOOST_FOREACH(int score, play(p1, p2, num_rounds))
    {
        results.append(score);
    }

    return results;
}

struct Round_to_tuple
{
    static PyObject* convert(const Round& r)
        {
            return bp::incref<>(
                bp::make_tuple(r.p1, r.p2).ptr());
        }
};

struct Round_from_tuple
{
    Round_from_tuple()
        {
            bp::converter::registry::push_back(
                &convertible,
                &construct,
                bp::type_id<Round>());
        }

    static bool checkIsMove(PyObject* obj) {
        bp::object move_obj = bp::import("rps").attr("Move");
        return PyObject_IsInstance(obj, move_obj.ptr());
    }

    // Determine if obj_ptr can be converted in a Round
    static void* convertible(PyObject* obj_ptr)
        {
            if (!PyTuple_Check(obj_ptr)) return 0;
            if (PyTuple_Size(obj_ptr) != 2) return 0;
            if (!checkIsMove(PyTuple_GetItem(obj_ptr, 0))) return 0;
            if (!checkIsMove(PyTuple_GetItem(obj_ptr, 1))) return 0;

            return obj_ptr;
        }

    // Convert obj_ptr into a Round
    static void construct(
        PyObject* obj_ptr,
        bp::converter::rvalue_from_python_stage1_data* data)
        {
            // Extract Move values from tuple
            Move m1 = bp::extract<Move>(
                bp::object(
                    bp::handle<>(
                        bp::borrowed(
                            PyTuple_GetItem(obj_ptr, 0)))));

            Move m2 = bp::extract<Move>(
                bp::object(
                    bp::handle<>(
                        bp::borrowed(
                            PyTuple_GetItem(obj_ptr, 1)))));

            // Grab pointer to memory into which to construct the new Round
            void* storage = (
                (bp::converter::rvalue_from_python_storage<Round>*)
                data)->storage.bytes;

            // in-place construct the new Round using the character data
            // extraced from the python object
            new (storage) Round(m1, m2);

            // Stash the memory chunk pointer for later use by boost.python
            data->convertible = storage;
        }
};

class PlayerWrap : public Player,
                   public bp::wrapper<Player>
{
public:
    PlayerWrap(const std::string& name) :
        Player(name)
        {}

    Move nextMove(const std::vector<Round>& history,
                  unsigned char my_pos) const
        {
            bp::list py_hist;
            BOOST_FOREACH(const Round& r, history) {
                py_hist.append(r);
            }

            return this->get_override("next_move")(py_hist, my_pos);
        }
};

BOOST_PYTHON_MODULE(rps)
{
    // register the to-python converter for rounds
    bp::to_python_converter<
        Round,
        Round_to_tuple>();

    // register the from-python converter rounds
    Round_from_tuple();

    bp::def("test", test, test_overloads());

    bp::class_<PlayerWrap, boost::noncopyable>(
        "Player", bp::init<const std::string&>())
        .add_property("name", &PlayerWrap::name, &PlayerWrap::setName)
        ;

    bp::class_<Random, bp::bases<Player> >(
        "Random",
        boost::python::init<const std::string&>())
        ;

    bp::class_<TitForTat, bp::bases<Player> >(
        "TitForTat",
        boost::python::init<const std::string&>())
        ;

    bp::enum_<Move>("Move")
        .value("Rock", Rock)
        .value("Paper", Paper)
        .value("Scissors", Scissors)
        ;

    bp::def("score", score2);

    bp::def("play", py_play);
}
