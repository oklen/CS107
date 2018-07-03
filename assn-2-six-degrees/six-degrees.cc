#include <vector>
#include <list>
#include <set>
#include <string>
#include <iostream>
#include <iomanip>
#include "imdb.h"
#include "path.h"
#include "mypath.h"

using namespace std;

/**
 * Using the specified prompt, requests that the user supply
 * the name of an actor or actress.  The code returns
 * once the user has supplied a name for which some record within
 * the referenced imdb existsif (or if the user just hits return,
 * which is a signal that the empty string should just be returned.)
 *
 * @param prompt the text that should be used for the meaningful
 *               part of the user prompt.
 * @param db a reference to the imdb which can be used to confirm
 *           that a user's response is a legitimate one.
 * @return the name of the user-supplied actor or actress, or the
 *         empty string.
 */

static string promptForActor(const string& prompt, const imdb& db)
{
  string response;
  while (true) {
    cout << prompt << " [or <enter> to quit]: ";
    getline(cin, response);
    if (response == "") return "";
    vector<film> credits;
    if (db.getCredits(response, credits)) return response;
    cout << "We couldn't find \"" << response << "\" in the movie database. "
	 << "Please try again." << endl;
  }
}

/**
 * Serves as the main entry point for the six-degrees executable.
 * There are no parameters to speak of.
 *
 * @param argc the number of tokens passed to the command line to
 *             invoke this executable.  It's completely ignored
 *             here, because we don't expect any arguments.
 * @param argv the C strings making up the full command line.
 *             We expect argv[0] to be logically equivalent to
 *             "six-degrees" (or whatever absolute path was used to
 *             invoke the program), but otherwise these are ignored
 *             as well.
 * @return 0 if the program ends normally, and undefined otherwise.
 */

int main(int argc, const char *argv[])
{
  imdb db(determinePathToData(argv[1])); // inlined in imdb-utils.h
  if (!db.good()) {
    cout << "Failed to properly initialize the imdb database." << endl;
    cout << "Please check to make sure the source files exist and that you have permission to read them." << endl;
    exit(1);
  }
  
  
  while (true) {
    string source = promptForActor("Actor or actress", db);
    if (source == "") break;
    string target = promptForActor("Another actor or actress", db);
    if (target == "") break;

    // string source("Kevin Bacon");
    // string target("E.E. Bell");
    if (source == target) {
      cout << "Good one.  This is only interesting if you specify two different people." << endl;
    } else {
      // replace the following line by a call to your generateShortestPath routine...
      vector<film> currentFileV;
      vector<film> currentFileV2;
      db.getCredits(source, currentFileV);
      db.getCredits(target, currentFileV2);

      if(currentFileV.size() > currentFileV.size())
        {
          swap(source,target);
          currentFileV = currentFileV2;
          currentFileV2.~vector();
        }
      list<actorNode> actorStore; // Used to put used actor
      list<movieNode> movieStore; // Used to put used movie
      list<actorNode*> PathStore;  // Awesome Looking Up Paths

      path ph{source};
      ph.usedPlayer.insert(source);
      
      actorStore.push_back(actorNode(source));
      PathStore.push_back(new actorNode(source));

      auto LastEnd = PathStore.begin();
      //int oldPathLength = 1;
      for(int deepCount = 1; deepCount < 7;deepCount++)
        {
          auto paths = LastEnd;
          auto OnceEnd = PathStore.end();
          OnceEnd--;
          bool MoreOnce = true;
          int cachesize = PathStore.size();
          while(paths != OnceEnd || MoreOnce)
            {
              if(paths==OnceEnd)
                MoreOnce=!MoreOnce;
              auto cacheMovie{vector<film>()};
              db.getCredits((*paths)->actorName,cacheMovie);
              auto moveIt = cacheMovie.begin();
              while(moveIt != cacheMovie.end())
                {
                  if(ph.usedMovie.insert((*moveIt)).second)
                    {
                      movieStore.push_back(movieNode(*moveIt, *paths));
                      auto cacheActor {vector<string>()};
                      db.getCast(*moveIt, cacheActor);
                      auto actorIt = cacheActor.begin();
                      while(actorIt != cacheActor.end())
                        {
                          if(ph.usedPlayer.insert(*actorIt).second)
                            {
                              //actorStore.push_back(actorNode(*actorIt,&movieStore.back()));
                              PathStore.push_back(new actorNode(*actorIt,
                                                              &movieStore.back()));
                              if(*actorIt == target)
                                {
                                  goto wefindit;
                                }
                            }
                          actorIt++;
                        }
                    }
                  moveIt++;
                }
              paths++;
            }
          auto it = PathStore.begin();
          while(cachesize > 0)
            {
              it++;
              cachesize--;
            }
          LastEnd = it;
        }
      cout << endl << "No path between those two people could be found." << endl << endl;
      continue;
    wefindit:
      cout << "We Find by:";
      auto GetsPath = PathStore.back();
      while(true)
        {
          GetsPath->show();
          if(GetsPath->ptrtoFilm != nullptr)
            GetsPath = GetsPath->ptrtoFilm->show();
          else
            {
              cout << endl;
              break;
            }
        }
    }
  }
  
  cout << "Thanks for playing!" << endl;
  return 0;
}

