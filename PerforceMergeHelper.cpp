// IntegrateHelper.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <windows.h>


#define ENSURE( condition, doWhenFail )                                        \
if ( !(condition) )                                                            \
{                                                                              \
    printf( "[UX+] %s, Error occurred with '%s'.", __FUNCTION__, #condition ); \
    doWhenFail;                                                                \
}


std::string perforceHost;
int         perforcePort;
std::string perforceUserId;
std::string perforceUserPw;
std::string perforceWorkspace;


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	returns branch mapping between branch1 and branch2
///
/// @param	branch1	first branch
/// @param	branch2	second branch
/// @param	whether or not it's reverse
///
/// @return	branch mapping
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string GetBranchMapping( std::string& branch1, std::string& branch2, bool& reverse )
{
	char buf[ 256 ];
	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C utf8 -c %s -p %s:%d -u %s -P %s branches > log.txt",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str() );
	system( buf );

	std::list< std::string > branchNameList;
	FILE* file = fopen( "log.txt", "r" );
	while ( fgets( buf, sizeof( buf ), file ) )
	{
		char* token = strtok( buf, " " );
		char* branchName = strtok( nullptr, " " );
		branchNameList.push_back( branchName );
	}
	fclose( file );

	for ( const std::string& branchName : branchNameList )
	{
		std::size_t pos1 = branchName.find( branch1 );
		if ( pos1 == std::string::npos ) continue;

		std::size_t pos2 = branchName.find( branch2 );
		if ( pos2 == std::string::npos ) continue;

		reverse = (pos1 > 0);
		return branchName;
	}

	return "invalid_branch";
}

void SetClipboard( const std::string& s )
{
	HWND hwnd = GetDesktopWindow();
	OpenClipboard( hwnd );
	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc( GMEM_MOVEABLE, s.size() + 1 );
	if ( !hg ) {
		CloseClipboard();
		return;
	}
	memcpy( GlobalLock( hg ), s.c_str(), s.size() + 1 );
	GlobalUnlock( hg );
	SetClipboardData( CF_TEXT, hg );
	CloseClipboard();
	GlobalFree( hg );
}

int main()
{
	FILE* configFile = fopen( "config.txt", "r" );
	if ( !configFile ) return 1;

	char        buf[ 1024 * 100 ];
	std::string name;
	std::unordered_map< std::string, std::string > branchMap;
	while ( fgets( buf, sizeof( buf ) - 1, configFile ) )
	{
		char* token = strtok( buf, " \t" );
		if ( !token ) continue;

		if ( !strcmp( token, "name" ) )
			name = strtok( nullptr, " \t\r\n" );
		else if ( !strcmp( token, "perforce_host" ) )
			perforceHost = strtok( nullptr, " \t\r\n" );
		else if ( !strcmp( token, "perforce_port" ) )
			perforcePort = atoi( strtok( nullptr, " \t\r\n" ) );
		else if ( !strcmp( token, "perforce_user_id" ) )
			perforceUserId = strtok( nullptr, " \t\r\n" );
		else if ( !strcmp( token, "perforce_user_pw" ) )
			perforceUserPw = strtok( nullptr, " \t\r\n" );
		else if ( !strcmp( token, "perforce_workspace" ) )
			perforceWorkspace = strtok( nullptr, " \t\r\n" );
		else if ( !strcmp( token, "branch" ) )
		{
			std::string branchName = strtok( nullptr, " \t" );
			std::string branchPath = strtok( nullptr, " \t\r\n" );
			branchMap[ branchName ] = branchPath;
		}
	}

	fclose( configFile );

	printf( "revision : " );

	gets_s( buf );

	int revision = atoi( buf );

	std::string srcBranch = "None";

	printf( "target branch(def: JP_Dev) : " );
	gets_s( buf );

	std::string dstBranch;
	if ( *buf )
		dstBranch = buf;
	else
		dstBranch = "JP_Dev";

	std::string personName;

	sprintf_s( buf, sizeof( buf ) - 1, "p4 describe -s %d > log.txt", revision );
	system( buf );

	FILE* logFile = fopen( "log.txt", "r" );
	ENSURE( logFile, return 1 );

	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		if ( !strstr( buf, "... //depot/" ) ) continue;

		for ( auto& pair : branchMap )
		{
			if ( strstr( buf, pair.second.c_str() ) )
			{
				srcBranch = pair.first;
				break;
			}
		}

		break;
	}

	ENSURE( srcBranch != "None", return 1 );
	fclose( logFile );
	logFile = fopen( "log.txt", "r" );
	ENSURE( logFile, return 1 );

	ENSURE( fgets( buf, sizeof( buf ) - 1, logFile ), return 1 );
	ENSURE( fgets( buf, sizeof( buf ) - 1, logFile ), return 1 );

	std::string srcComment;
	std::string comment;
	char        branchInfo[ 256 ] = "";
	std::string readBranch = "None";
	
	bool revisionCommentProcessed = false;
	bool ignore                   = false;
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		char* p = strtok( buf, "\r\n" );
		if ( !p ) continue;
		if ( !strcmp( p, "Affected files ..." ) )
			ignore = true;

		srcComment += std::string( "# " ) + p + "\r\n";
		if ( ignore ) continue;

		if ( readBranch == "None" )
		{
			char* p = buf;
			while ( *p != '[' ) p++;
			++p;

			char* start = p;
			while ( *p != ']' ) p++;
			personName = std::string( start, p - start );
			++p;

			if ( *p == '[' )
			{
				++p;
				if ( strstr( buf, "=>" ) )
				{
					while ( *p != '>' ) p++;
					while ( *p != ' ' ) p++;
					++p;
				}
				char* end = p;
				while ( *end != ']' ) end++;

				std::string branchName( p, end - p );
				readBranch = branchName;
				ENSURE( readBranch != "None", return 1 );

				if ( srcBranch == "None" )
					srcBranch = readBranch;

				p = end + 1;
			}
			else
			{
				srcBranch = "Trunk";
			}

			p = strtok( p, "\r\n" );
			if ( !p || !*p ) continue;

			strcpy( buf, p );
		}

		if ( !revisionCommentProcessed )
		{
			revisionCommentProcessed = true;

			if ( char* p = strstr( buf, "- #" ) )
			{
				p += 3;
				while ( *p != ' ' ) p++;
				++p;
				comment += p;
			}
			else if ( char* p = strstr( buf, "- @" ) )
			{
				p += 3;
				while ( *p != ' ' ) p++;
				++p;
				comment += p;
			}
			else if ( char* p = strstr( buf, "- " ) )
			{
				p += 2;
				comment += p;
			}
			else
			{
				comment += (buf + 1);
			}
		}
		else
		{
			comment += (buf + 1);
		}

		comment += "\r\n";
	}

	//comment[ comment.length() - 1 ] = 0;

	fclose( logFile );

	printf( "old_comment : %s\n", comment.c_str() );
	printf( "person      : %s\n", personName.c_str() );
	printf( "src_branch  : %s\n", srcBranch.c_str() );
	printf( "dst_branch  : %s\n", dstBranch.c_str() );
	
	sprintf_s(
		buf, sizeof( buf ) - 1,
		"[%s][%s => %s]\n"
		"- #%d %s",
		name.c_str(),
		srcBranch.c_str(), dstBranch.c_str(),
		revision, comment.c_str() );
	std::string newComment = buf;
	printf( "new_comment\n%s\n", newComment.c_str() );

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"%s\r\n"
		"Change: new\r\n\r\n"
		"Description:\r\n"
		"\t[%s][%s => %s]\r\n"
		"\t- #%d %s%s",
		srcComment.c_str(),
		name.c_str(),
		srcBranch.c_str(), dstBranch.c_str(),
		revision, personName != name ? ("[" + personName + "]").c_str() : "", comment.c_str() );
	SetClipboard( buf );
	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C cp949 -c %s -p %s:%d -u %s -P %s change > log.txt",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str() );
	system( buf );

	logFile = fopen( "log.txt", "r" );
	ENSURE( logFile, return 1 );
	fgets( buf, sizeof( buf ) - 1, logFile );
	char* p = buf;
	while ( *p != ' ' ) ++p;
	++p;

	int newChangeListNo = atoi( p );
	printf( "newChangeList: %d\n", newChangeListNo );
	fclose( logFile );

	bool        reverse;
	std::string branchMapping = GetBranchMapping( srcBranch, dstBranch, reverse );

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C utf8 -c %s -p %s:%d -u %s -P %s integrate -c %d %s-b \"%s\" -s //depot/%s/...@%d,@%d",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str(),
		newChangeListNo,
		reverse ? "-r " : "",
		branchMapping.c_str(),
		branchMap[ srcBranch ].c_str(),
		revision, revision );
	system( buf );

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C utf8 -c %s -p %s:%d -u %s -P %s change -o %d > log.txt",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str(),
		newChangeListNo );
	system( buf );

	logFile = fopen( "log.txt", "r" );
	ENSURE( logFile, return 1 );
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		char* p = strtok( buf, "\r\n" );
		if ( !p ) continue;
		if ( !strcmp( p, "Files:" ) )
			break;
	}

	int autoResolveFailedCount = 0;
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		char* p = strtok( buf, "\t\r\n" );
		if ( !p ) continue;

		char command[ 256 ];
		sprintf_s(
			command, sizeof( command ) - 1,
			"p4 -C utf8 -c %s -p %s:%d -u %s -P %s resolve -o -am %s > log_resolve.txt",
			perforceWorkspace.c_str(),
			perforceHost.c_str(),
			perforcePort,
			perforceUserId.c_str(),
			perforceUserPw.c_str(),
			p );
		system( command );

		FILE* reolveLogFile = fopen( "log_resolve.txt", "r" );
		ENSURE( reolveLogFile, return 1 );
		ENSURE( fgets( buf, sizeof( buf ) - 1, reolveLogFile ), return 1 );
		ENSURE( fgets( buf, sizeof( buf ) - 1, reolveLogFile ), return 1 );
		bool autoResolved = (strstr( buf, "+ 0 conflicting" ) != nullptr);
		printf( "autoResolved: %s-%s\n", p, autoResolved ? "success" : "failure" );

		fclose( reolveLogFile );

		if ( !autoResolved )
			autoResolveFailedCount++;
	}

	fclose( logFile );

	if ( autoResolveFailedCount )
	{
		system( "pause" );
		return 0;
	}

	printf( "Summit!\n" );

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C utf8 -c %s -p %s:%d -u %s -P %s submit -c %d",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str(),
		newChangeListNo );
	system( buf );

	system( "pause" );

    return 0;
}
