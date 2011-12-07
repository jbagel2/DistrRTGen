// rtgen_client.cpp : Defines the entry point for the console application.
//
#include <fstream>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <time.h>

#ifndef WIN32
#include <errno.h>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CPU_INFO_FILENAME "/proc/cpuinfo"
#define MAX_PART_SIZE 8000000
#endif

#include "ClientSocket.h"
#include "config.h"
#include "Public.h"
#include "RainbowTableGenerator.h"

#define CLIENT_WAIT_TIME_SECONDS 5
#define VERSION "1.0"
using std::cout;
using std::endl;

CClientSocket *Con = new CClientSocket(SOCK_STREAM, 0, SERVER, PORT);

void End(int nSig)
{
	cout << endl;
	cout << "+-----------------------------+\n";
	Con->Close();
	exit(-1);
}

int main(int argc, char* argv[])
{
	int nResult;
	double nFrequency;
	std::string sHomedir;
	int nNumProcessors = 0;
	
	nFrequency = 0;
	#ifndef WIN32
	// Try to catch cpu Frequency from /proc/cpuinfo
	const char* cpuprefix = "cpu MHz";
	FILE* F;
	char cpuline[300+1];
	char* pos;
	int ok = 0;

	nNumProcessors = 0;

	signal(SIGINT, &End);

	// open cpuinfo system file
	F = fopen(CPU_INFO_FILENAME,"r");
	if (!F) return 0;

	//read lines
	while (!feof(F))
	{
		fgets (cpuline, sizeof(cpuline), F);
		// test if it's the frequency line
		if (!strncmp(cpuline, cpuprefix, strlen(cpuprefix)))
		{
			// Yes, grep the frequency
			pos = strrchr (cpuline, ':') +2;
			if (!pos) break;
			if (pos[strlen(pos)-1] == '\n') pos[strlen(pos)-1] = '\0';
			strcpy (cpuline, pos);
			strcat (cpuline,"e6");
			nFrequency = atof (cpuline)/1000000;
			ok = 1;
		}
	}
	nNumProcessors = sysconf(_SC_NPROCESSORS_ONLN);

	if (ok != 1)
	{
		cout << "| Cannot determine frequency  |" << endl;
		cout << "+-----------------------------+" << endl;
		nFrequency = 0;
	}
	#endif

	stWorkInfo stWork;

	// Check to see if there is something to resume from
	std::ostringstream sResumeFile;
	sResumeFile << ".resume";
	FILE *file = fopen(sResumeFile.str().c_str(), "rb");
	if(file != NULL)
	{
		// Bingo.. There is a resume file.
		fread(&stWork, sizeof(unsigned int), 6, file);
		fread(&stWork.nChainStart, sizeof(uint64), 1, file);
		char buf[8096];
		memset(buf, 0x00, sizeof(buf));
		fread(&buf[0], sizeof(buf), 1, file);
		fclose(file);
		char szCharset[8096], szHR[8096];
		strcpy(&szCharset[0], &buf[0]);
		stWork.sCharset.assign(szCharset);
		const char *pHR = strchr(&buf[0], 0x00);
		pHR++;
		strcpy(&szHR[0], pHR);
		stWork.sHashRoutine.assign(szHR);
		pHR = strchr(pHR, 0x00);
		pHR++;
		strcpy(&szHR[0], pHR);
		stWork.sSalt.assign(szHR);
		//before continuing, test if part file is <8MB sized
		const char * cFileName;
		std::string sFileName;
		std::stringstream szFileName;

		szFileName << stWork.nPartID << ".part";
		sFileName = szFileName.str();
		cFileName = sFileName.c_str();
		FILE *partfile = fopen(cFileName,"rb");
		if(partfile != NULL)
		{
			if( remove(cFileName) != 0 )
			{
				cout << "| Cannot delete .part file    |" << endl;
				cout << "+-----------------------------+" << endl;
				exit(1);
			}
		}
	}

	/* Main thread starting up */
	CRainbowTableGenerator *pGenerator = new CRainbowTableGenerator(nNumProcessors);
	//	          1          2
	//       123456789012345667890123
	cout << "+" << string(29,'-') << "+" << endl; // 24 Chars
	cout << "|     Starting RTCrack " << VERSION << "    |" << endl;
	cout.fill(' ');
	cout.width(28);
	cout << left << "| Processors: ";
	cout << right << pGenerator->GetProcessorCount() << " |" << endl;
	#ifndef WIN32
	cout.fill(' ');
	cout.width(25);
	cout << left << "| Frequency: ";
	cout << right << (int)nFrequency << " |" << endl;
	#endif
	cout << "+" << string(29,'-') << "+" << endl; // 24 Chars


	while(1)
	{
		#ifndef WIN32
		//renice main thread to 0.
		setpriority(PRIO_PROCESS, 0, 0);
		#endif
		// If there is no work to do, request some!
		if(stWork.sCharset == "")
		{
			cout << "+" << string(29,'-') << "+" << endl;
			cout << "| Requesting work...          |" << endl;
			int errorCode = Con->RequestWork(&stWork);

			while(errorCode > 0)
			{
				cout << "| No work. Retrying...        |" << endl;
				Sleep(CLIENT_WAIT_TIME_SECONDS*1000);
				errorCode = Con->RequestWork(&stWork);
			}

			FILE *fileResume = fopen(sResumeFile.str().c_str(), "wb");
			if(fileResume == NULL)
			{
				cout << "| Cannot create .resume       |" << endl;
				cout << "+-----------------------------+" << endl;
				exit(1);
			}
			fwrite(&stWork, sizeof(unsigned int), 6, fileResume); // Write the 6 unsigned ints
			fwrite(&stWork.nChainStart, 1, 8, fileResume); // Write nChainStart uint64
			fwrite(stWork.sCharset.c_str(), stWork.sCharset.length(), 1, fileResume);
			fputc(0x00, fileResume);
			fwrite(stWork.sHashRoutine.c_str(), stWork.sHashRoutine.length(), 1, fileResume);
			fclose(fileResume);

		}
		std::stringstream szFileName;
		szFileName << stWork.nPartID << ".part"; // Store it in the users home directory

		int nReturn;

		if((nReturn = pGenerator->CalculateTable(szFileName.str(), &stWork, &Con)) != 0)
		{
			cout << "| Generate failed...          |" << endl;
			cout << "+-----------------------------+" << endl;
			exit(nReturn);
		}
		szFileName << ".zip";

		cout << "+-----------------------------+" << endl;
		cout << "| Uploading...                |" << endl;


		nResult = -1;
		while(nResult != 0 && nResult != 1)
		{
			int nResult = Con->SendFinishedWork(szFileName.str());
			if(nResult == 0)
			{
				cout << "| Success!                    |" << endl;
				remove(szFileName.str().c_str());
				remove(szFileName.str().substr(0, szFileName.str().size()-4).c_str());
				stWork.sCharset = ""; // Blank out the charset to indicate the work is complete
			}
			else if(nResult == 1)
			{
				cout << "| Server reassigned part      |" << endl;
				remove(szFileName.str().c_str());
				remove(szFileName.str().substr(0, szFileName.str().size()-4).c_str());
				stWork.sCharset = ""; // Blank out the charset to indicate the work is complete
			}
			else
			{
				cout << "| Failure... Retrying         |" << endl;
				Sleep(CLIENT_WAIT_TIME_SECONDS * 1000);
			}
			break;
		}

		cout << "+-----------------------------+" << endl;
		cout << "| Part completed and uploaded |" << endl;
		cout << "+-----------------------------+" << endl;
		cout << endl;
	}
	return 0;
}
