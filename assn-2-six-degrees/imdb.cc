using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include "imdb.h"

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

imdb::imdb(const string& directory)
{
  cout << "Directory:" << directory << endl;
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;

  cout << actorFileName << endl;
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const
{
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

// you should be implementing these two methods right here... 
bool imdb::getCredits(const string& player, vector<film>& films) const {

  int low = 1;
  int hight = *((int*)actorFile);
  int mid = (low + hight + 1) /2;
  int current = 0;
  int result;
  while(hight >= low) //Binary search 
    {
      result = strcmp((const char*)((char*)actorFile +  *((int*)actorFile+mid)),player.c_str());
      if(result!= 0)
        {
          if(result > 0)
              hight = mid-1;
          else
            low = mid+1;
        }
      else
        {
          result = mid;
          goto wefind;
        }
      mid = (low + hight) /2;
    }
  return false;

 wefind:
  for(int i = (*((int*)actorFile + result)) ;i < *((int*)actorFile + result+1);i++)
    {
      if( *((char*)actorFile+i) != '\0')
        i;
      else
        {
          current = i - *((int*)actorFile + result);
          break;
        }
    }
 if(current%2!=0)
    current++;
  else
    current+=2;

  short moveCount = *(short*)((char*)actorFile+current + *((int*)actorFile + result));
  //cout << " Number of movies:" << moveCount << endl;
  if((current+2)%4 == 0)
    current+=2;
  else
    current+=4;

  //cout << *(int*)((char*)actorFile+ current  + *((int*)actorFile + result)) << endl;

  int* currentPtr = (int*)((char*)actorFile+ current + *((int*)actorFile + result));
  for(int i = 0; i < moveCount;i++)
    {
      string movieName(((char*)movieFile+*(currentPtr+i)));
      films.push_back(film(movieName,(short)*((char*)movieFile+*(currentPtr+i)+movieName.size()+1)));
      //cout <<  movieName<< " made in year:" << (1900 + films.back().year) << endl;
    }
  return true;
}

bool imdb::getCast(const film& movie, vector<string>& players) const {
  int low = 1;
  int hight = *((int*)movieFile);
  int mid = (low + hight + 1) /2;
  int current = 1;
  int result;
  while(hight >= low) //Binary search 
    {
      result = strcmp((const char*)((char*)movieFile +  *((int*)movieFile+mid)),movie.title.c_str());
      if(result!= 0)
        {
          if(result > 0)
            hight = mid-1;
          else
            low = mid+1;
        }
      else
        {
          int cmid = mid;
          while(strcmp((const char*)((char*)movieFile +  *((int*)movieFile+cmid)),movie.title.c_str()) == 0)
            {
              int Cachecount = 0;
          while(*((char*)movieFile +  *((int*)movieFile+cmid) + Cachecount) != '\0')
            Cachecount++;
          Cachecount++;
          if( (short)*((char*)movieFile +  *((int*)movieFile+cmid) + Cachecount)
              == movie.year)
            {
              result = cmid;
              goto wefind;
            }
          cmid++;
            }
          cmid = mid -1;
          while(strcmp((const char*)((char*)movieFile +  *((int*)movieFile+cmid)),movie.title.c_str()) == 0)
            {
              int Cachecount = 0;
              while(*((char*)movieFile +  *((int*)movieFile+cmid) + Cachecount) != '\0')
                Cachecount++;
              Cachecount++;
              if( (short)*((char*)movieFile +  *((int*)movieFile+cmid) + Cachecount)
                  == movie.year)
                {
                  result = cmid;
                  goto wefind;
                }
              cmid--;
            }
          cout << "Error In find movie by years" << endl;
          exit(0);
        }
      mid = (low + hight) /2;
    }
  return false;
 wefind:
  //cout << "Find:" <<
  //strdup((const char*)((char*)movieFile +  *((int*)movieFile+result))) << endl;

  char* currentPtr =  ((char*)movieFile +  *((int*)movieFile+result));
  current = movie.title.size() + 1;
  current+=1;
  if((current)%2) current++;
  short numberOfActor = *(short*)(currentPtr+current);
  //cout << "Get Actors number:" << numberOfActor << endl;
  current+=2;
  if(((current)%4))
    current+=2;
  

  for(int i =0; i < numberOfActor;i++)
    {
      auto actorPtr {(char*)actorFile + *(int*)(currentPtr+current)};

      //cout <<  *(int*)(currentPtr+current) << endl;
      //cout << "Get Actor:" <<  string(actorPtr) << endl;
      players.push_back(string(actorPtr));
      current+=4;
    }
    return false;
}

imdb::~imdb()
{
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// ignore everything below... it's all UNIXy stuff in place to make a file look like
// an array of bytes in RAM.. 
const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info)
{
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info)
{
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}
