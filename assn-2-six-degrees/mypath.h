#include "imdb.h"
struct movieNode;

struct actorNode
{
public:
  actorNode(string actorName):actorName{actorName},ptrtoFilm{nullptr}{}
  actorNode(string actorName,movieNode *ptr):actorName{actorName},
                                             ptrtoFilm{ptr}{}
  string actorName;
  struct movieNode * ptrtoFilm;
  void show()
  {
    if(ptrtoFilm != nullptr)
      cout  << actorName << " -> ";
    else
      cout << actorName;
  }
};

struct movieNode
{
public:
  movieNode(film movie):movie{movie}{}
  movieNode(film movie,actorNode* ptr):movie{movie},ptrtoActor{ptr}{}
  film movie;
  actorNode* ptrtoActor;
  actorNode* show()
  {
    cout << movie.title << " -> ";
    return ptrtoActor;
  }
};


