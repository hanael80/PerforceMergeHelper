// IntegrateHelper.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <map>
#include <set>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <windows.h>


#pragma warning( disable: 4503 )


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
int         mode;
bool        utf8Mode = true;

/// BranchMap typedef
typedef std::unordered_map< std::string, std::string > BranchMap;

/// AccountIgnoreSet typedef
typedef std::set< std::string > AccountIgnoreSet;


enum class Mode
{
	Search,
	Merge,
	BulkMerge,
	MergeJob,
	SearchAndMerge,
	Revision,
	Max
};

class RevisionInfo
{
public:
	int                      num;
	std::string              date;
	std::string              comment;
	std::list< std::string > fileList;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	convert utf8 to ansi string
///
/// @param	utf8str	utf string
///
/// @return	ansi string
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string utf8_to_ansi( const std::string& utf8str )
{
	int srcLen = (int)( utf8str.length() );
    int length = MultiByteToWideChar( CP_UTF8, 0, utf8str.c_str(), srcLen + 1, 0, 0 );
	if ( length <= 0 ) return "";

    std::wstring wtemp( length, (wchar_t)( 0 ) );
    MultiByteToWideChar( CP_UTF8, 0, utf8str.c_str(), srcLen + 1, &wtemp[ 0 ], length );
    length = WideCharToMultiByte( CP_ACP, 0, &wtemp[ 0 ], -1, 0, 0, 0, 0 );
    std::string temp(length, (char)( 0 ) );
    WideCharToMultiByte( CP_ACP, 0, &wtemp[ 0 ], -1, &temp[ 0 ], length, 0, 0 );
    return temp;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	convert ansi to utf8 string
///
/// @param	ansiStr	ansi string
///
/// @return	utf8 string
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string ansi_to_utf8( const std::string& ansiStr )
{
	int srcLen = (int)( ansiStr.length() );
    int length = MultiByteToWideChar( CP_ACP, 0, ansiStr.c_str(), srcLen + 1, 0, 0 );
	if ( length <= 0 ) return "";

    std::wstring wtemp( length, (wchar_t)( 0 ) );
    MultiByteToWideChar( CP_ACP, 0, ansiStr.c_str(), srcLen + 1, &wtemp[ 0 ], length );
    length = WideCharToMultiByte( CP_UTF8, 0, &wtemp[ 0 ], -1, 0, 0, 0, 0 );
    std::string temp(length, (char)( 0 ) );
    WideCharToMultiByte( CP_UTF8, 0, &wtemp[ 0 ], -1, &temp[ 0 ], length, 0, 0 );
    return temp;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	run perforce command
///
/// @param	command		command to run
/// @param	logFileName	if logFileName is assigned, returns a file pointer
///						which contains the result of the command
///
/// @return	file pointer which contains the result of the command
////////////////////////////////////////////////////////////////////////////////////////////////////
FILE* p4( const std::string& command, const char* logFileName = "log.txt" )
{
	char buf[ 1024 * 100 ];

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C %s -c %s -p %s:%d -u %s -P %s %s %s%s",
		utf8Mode ? "utf8" : "cp949",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str(),
		command.c_str(),
		logFileName ? "> " : "",
		logFileName ? logFileName : "" );
	system( buf );

	if ( !logFileName ) return nullptr;

	FILE* file = fopen( logFileName, "r" );
	return file;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	returns branch mapping between branch1 and branch2
///
/// @param	branch1	first branch
/// @param	branch2	second branch
/// @param	whether or not it's reverse
///
/// @return	branch mapping
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string GetBranchMapping( const std::string& branch1, const std::string& branch2, bool& reverse )
{
	FILE* file = p4( "branches" );
	ENSURE( file, return "invalid_branch" );

	std::list< std::string > branchNameList;

	char buf[ 1024 * 100 ];
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

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	reads config
///
/// @param	name				name of user
/// @param	branchMap			map of branches
/// @param	accountIgnoreMap	accounts to ignore
///
/// @return	success or failure
////////////////////////////////////////////////////////////////////////////////////////////////////
bool read_config(
	std::string&                                    name,
	std::unordered_map< std::string, std::string >& branchMap,
	AccountIgnoreSet&                               accountIgnoreSet )
{
	FILE* configFile = fopen( "config.txt", "r" );
	if ( !configFile ) return false;

	char buf[ 1024 * 100 ];
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
		else if ( !strcmp( token, "account_to_ignore" ) )
		{
			std::string accountToIgnore = strtok( nullptr, " \t\r\n" );
			accountIgnoreSet.insert( accountToIgnore );
		}
	}

	fclose( configFile );

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	searches revisions which contain a specific text
///
/// @return	no return
////////////////////////////////////////////////////////////////////////////////////////////////////
void search()
{
	printf( "folder: " );
	char buf[ 1024 * 100 ];
	gets_s( buf );
	std::string folder = buf;

	printf( "keyword: " );
	gets_s( buf );

	std::string keyword = buf;

	sprintf_s( buf, sizeof( buf ) - 1, "changes -s submitted -l -m 3000 //depot/%s...", folder.c_str() );
	FILE* logFile = p4( buf );
	ENSURE( logFile, return );

	int         readMode = 1;
	int         revision;
	std::string comment;
	bool        properRevision;

	std::map< int, std::string > revisionMap;
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		switch ( readMode )
		{
		case 1:
			{
				char* p = strtok( buf, " " );
				ENSURE( !strcmp( p, "Change" ), return );

				p = strtok( nullptr, " " );
				revision = atoi( p );
				ENSURE( fgets( buf, sizeof( buf ) - 1, logFile ), return );
				readMode = 2;
				comment = "";
				properRevision = false;
			}
			break;
		case 2:
			{
				if ( *buf != '\t' )
				{
					if ( properRevision )
						revisionMap[ revision ] = comment;

					readMode = 1;
					break;
				}

				comment += buf;
				if ( strstr( buf, keyword.c_str() ) )
					properRevision = true;
			}
			break;
		}
	}

	for ( auto& pair : revisionMap )
	{
		printf( "-------------------------------------------------------------\n" );
		printf( "revision: %d\n", pair.first );
		printf( "%s\n", pair.second.c_str() );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	views revision
///
/// @return	no returns
////////////////////////////////////////////////////////////////////////////////////////////////////
void view_revision()
{
	printf( "revision: " );
	char buf[ 1024 * 100 ];
	gets_s( buf );
	int revision = atoi( buf );
	sprintf_s( buf, sizeof( buf ) - 1, "p4vc change %d", revision );
	system( buf );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	test integration
///
/// @param	reverse			use branch mapping direction in reverse
/// @param	branchMapping	name of branch mapping
/// @param	branchMap		map of branches
/// @param	srcBranch		name of source branch
/// @param	revision		revision number
///
/// @return	no returns
////////////////////////////////////////////////////////////////////////////////////////////////////
void test_integration(
	      bool          reverse,
	const std::string&  branchMapping,
	      BranchMap&    branchMap,
	const std::string&  srcBranch,
	      int           revision )
{
	char buf[ 1024 * 10 ];
	sprintf_s(
		buf, sizeof( buf ) - 1,
		"integrate %s-n -b \"%s\" -s //depot/%s/...@%d,@%d",
		reverse ? "-r " : "",
		branchMapping.c_str(),
		branchMap[ srcBranch ].c_str(),
		revision, revision );
	p4( buf, nullptr );
	system( "pause" );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	parse a submitter's name
///
/// @param	p	string pointer
///
/// @return	submitter's name
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string parse_submitter_name( char*& p )
{
	while ( *p != '[' && *p ) p++;
	if ( !*p ) return "";

 	++p;

	char* start = p;
	while ( *p != ']' ) p++;
	++p;

	return std::string( start, p - start - 1 );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	parse a submitter's name
///
/// @param	buf			buffer
/// @param	p			string pointer after parsing
/// @param	readBranch	branch name which is read this time
/// @param	srcBranch	branch name which is determined as a source branch
///
/// @return	no returns
////////////////////////////////////////////////////////////////////////////////////////////////////
void parse_branch_name( char* buf, char*& p, std::string& readBranch, std::string& srcBranch )
{
	if ( *p != '[' )
	{
		readBranch = "Trunk";
		if ( srcBranch == "None" )
			srcBranch  = "Trunk";
		return;
	}

	++p;
	if ( strstr( buf, "=>" ) )
	{
		while ( *p != '>' ) p++;
		if ( *p == ' ' )
			while ( *p != ' ' ) p++;
		++p;
	}
	char* end = p;
	while ( *end != ']' ) end++;

	std::string branchName( p, end - p );
	readBranch = branchName;
	ENSURE( readBranch != "None", return );

	if ( srcBranch == "None" )
		srcBranch = readBranch;

	p = end + 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	parse a tag
///
/// @param	buf		buffer
/// @param	p		string pointer after parsing
/// @param	tag		name of tag
///
/// @return	no returns
////////////////////////////////////////////////////////////////////////////////////////////////////
void parse_tag( char* buf, char*& p, std::string& tag )
{
	if ( *p != '[' )
	{
		tag = "";
		return;
	}

	++p;
	char* end = p;
	while ( *end != ']' ) end++;

	tag = std::string( p, end - p );

	p = end + 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	perform merge
///
/// @param	name					name
/// @param	branchMap				map of branch
/// @param	accountIgnoreSet		accounts to ignore
/// @param	revision				number of revision
/// @param	newChangeListNo			new changelist no
/// @param	srcBranch				source branch
/// @param	dstBranch				target branch
/// @param	branchMapping			branch mapping to use when intergrating
/// @param	branchMappingReserve	whether or not, direction of the branch mapping is reserve
/// @param	testMode				whether or not, it's test mode
/// @param	autoSubmit				whether or not, automatically submit if there is no conflict
///
/// @return	result code
////////////////////////////////////////////////////////////////////////////////////////////////////
int perform_merge(
	const std::string&      name,
	      BranchMap&        branchMap,
	const AccountIgnoreSet& accountIgnoreSet,
	      int               revision,
	      int               newChangeListNo,
	      std::string       srcBranch,
	const std::string&      dstBranch,
	      std::string       branchMapping,
	      bool              branchMappingReverse,
	      bool              testMode,
	      bool              autoSubmit )
{
	std::string personName;

	char buf[ 1024 * 100 ];
	sprintf_s( buf, sizeof( buf ) - 1, "describe -s %d", revision );

	FILE* logFile = p4( buf );
	ENSURE( logFile, return 1 );

	bool srcBranchSpecified = !srcBranch.empty();
	if ( !srcBranchSpecified )
	{
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

		ENSURE( !srcBranch.empty(), return 1 );
		fclose( logFile );
		logFile = fopen( "log.txt", "r" );
		ENSURE( logFile, return 1 );
	}

	ENSURE( fgets( buf, sizeof( buf ) - 1, logFile ), return 1 );
	const char* token = " by ";
	char* p = strstr( buf, token );
	ENSURE( p, return 1 );
	p = strtok( p + strlen( token ), "@" );
	ENSURE( p, return 1 );
	std::string submitterAccount = p;
	if ( accountIgnoreSet.find( submitterAccount ) != accountIgnoreSet.end() )
	{
		fclose( logFile );
		return 0;
	}

	ENSURE( fgets( buf, sizeof( buf ) - 1, logFile ), return 1 );

	std::string srcComment;
	std::string comment;
	std::list< std::string > commentList;
	char        branchInfo[ 256 ] = "";
	std::string readBranch = "None";
	std::string tag;

	bool revisionCommentProcessed = false;
	bool inJobsFixed              = false;
	bool ignore                   = false;
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		char* p = strtok( buf, "\r\n" );
		if ( !p ) continue;
		if ( !strcmp( buf, "Jobs fixed ..." ) )
			inJobsFixed = true;

		if ( !strcmp( p, "Affected files ..." ) )
		{
			inJobsFixed = false;
			ignore      = true;
		}

		if ( inJobsFixed ) continue;

		srcComment += std::string( "# " ) + p + "\r\n";
		if ( ignore ) continue;

		if ( readBranch == "None" )
		{
			char* p = buf;
			personName = parse_submitter_name( p );
			if ( !personName.empty() )
			{
				parse_branch_name( buf, p, readBranch, srcBranch );
				parse_tag( buf, p, tag );

				p = strtok( p, "\r\n" );
				if ( !p || !*p ) continue;

				strcpy( buf, p );
			}
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
				commentList.push_back( p );
			}
			else if ( char* p = strstr( buf, "- @" ) )
			{
				p += 3;
				while ( *p != ' ' ) p++;
				++p;
				comment += p;
				commentList.push_back( p );
			}
			else if ( char* p = strstr( buf, "- " ) )
			{
				p += 2;
				comment += p;
				commentList.push_back( p );
			}
			else
			{
				comment += (buf + 1);
				commentList.push_back( buf + 1 );
			}
		}
		else
		{
			comment += (buf + 1);
			commentList.push_back( buf + 1 );
		}

		comment += "\r\n";
	}

	//comment[ comment.length() - 1 ] = 0;

	fclose( logFile );

// 	if ( utf8Mode )
// 	{
// 		srcComment = utf8_to_ansi( srcComment.c_str() );
// 		comment    = utf8_to_ansi( comment.c_str() );
// 		personName = utf8_to_ansi( personName.c_str() );
// 		tag        = utf8_to_ansi( tag.c_str() );
// 		for ( std::string& comment : commentList )
// 			comment = utf8_to_ansi( comment.c_str() );
// 	}

	printf( "old_comment : %s\n", utf8Mode ? utf8_to_ansi( comment    ).c_str() : comment.c_str()    );
	printf( "person      : %s\n", utf8Mode ? utf8_to_ansi( personName ).c_str() : personName.c_str() );
	printf( "src_branch  : %s\n", utf8Mode ? utf8_to_ansi( srcBranch  ).c_str() : srcBranch.c_str()  );
	printf( "dst_branch  : %s\n", utf8Mode ? utf8_to_ansi( dstBranch  ).c_str() : dstBranch.c_str()  );
	
	if ( branchMapping.empty() )
		branchMapping = GetBranchMapping( srcBranch, dstBranch, branchMappingReverse );

	if ( testMode )
	{
		test_integration( branchMappingReverse, branchMapping, branchMap, srcBranch, revision );
		return 0;
	}

	std::string tagPart;
	if ( !tag.empty() )
	{
		sprintf_s( buf, sizeof( buf ) - 1, "[%s]", tag.c_str() );
		tagPart = buf;
	}

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"[%s][%s => %s]%s\n"
		"- @%d %s",
		utf8Mode ? ansi_to_utf8( name ).c_str() : name.c_str(),
		srcBranch.c_str(), dstBranch.c_str(), tagPart.c_str(),
		revision, comment.c_str() );
	std::string newComment = buf;
	printf( "new_comment\n%s\n", utf8Mode ? utf8_to_ansi( newComment ).c_str() : newComment.c_str() );

	bool dstChangeListNoSpecified = (newChangeListNo > 0);
	if ( !newChangeListNo )
	{
		FILE* inputFile = fopen( "input.txt", "w" );
		fprintf_s( inputFile, "%s\r\n", srcComment.c_str() );
		fprintf_s(
			inputFile,
			"Change: new\r\n\r\n"
			"Description:\r\n"
			"\t[%s][%s => %s]%s\r\n"
			"\t- @%d %s",
			utf8Mode ? ansi_to_utf8( name ).c_str() : name.c_str(),
			srcBranch.c_str(), dstBranch.c_str(), tagPart.c_str(),
			revision, (utf8Mode ? utf8_to_ansi( personName ).c_str() : personName) != name ? ("[" + personName + "]").c_str() : "" );

		int lineIndex = 0;
		for ( const std::string& commentLine : commentList )
		{
			if ( lineIndex++ > 0 )
				fprintf_s( inputFile, "\r\n\t" );

			fprintf_s( inputFile, commentLine.c_str() );
		}

		fclose( inputFile );

		logFile = p4( "change -i < input.txt" );
		ENSURE( logFile, return 1 );
		fgets( buf, sizeof( buf ) - 1, logFile );
		p = buf;
		while ( *p != ' ' ) ++p;
		++p;

		newChangeListNo = atoi( p );
		printf( "newChangeList: %d\n", newChangeListNo );
		fclose( logFile );
	}

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"integrate -c %d %s-b \"%s\" -s //depot/%s/...@%d,@%d",
		newChangeListNo,
		branchMappingReverse ? "-r " : "",
		branchMapping.c_str(),
		branchMap[ srcBranch ].c_str(),
		revision, revision );
	p4( buf, false );

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"change -o %d",
		newChangeListNo );

	logFile = p4( buf );
	ENSURE( logFile, return 1 );
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		char* p = strtok( buf, "\r\n" );
		if ( !p ) continue;
		if ( !strcmp( p, "Files:" ) )
			break;
	}

	int  autoResolveSuccessCount = 0;
	bool autoResolveFailed       = false;
	std::list< std::string > fileList;
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		char* p = strtok( buf, "\t\r\n" );
		if ( !p ) continue;

		fileList.push_back( p );
	}

	for ( const std::string& filePath : fileList )
	{
		char command[ 256 ];
		sprintf_s(
			command, sizeof( command ) - 1,
			"resolve -o -am %s",
			filePath.c_str() );

		FILE* reolveLogFile = p4( command, "log_resolve.txt" );
		ENSURE( reolveLogFile, return 1 );
		bool autoResolved = true;
		if ( fgets( buf, sizeof( buf ) - 1, reolveLogFile ) )
		{
			ENSURE( fgets( buf, sizeof( buf ) - 1, reolveLogFile ), return 1 );
			autoResolved = (strstr( buf, "+ 0 conflicting" ) != nullptr);
		}
		//printf( "autoResolved: %s-%s\n", p, autoResolved ? "success" : "failure" );
		fclose( reolveLogFile );

		if ( !autoResolved )
		{
			autoResolveFailed = true;
			break;
		}
		else
		{
			autoResolveSuccessCount++;
		}
	}

	fclose( logFile );

	if ( dstChangeListNoSpecified ) return 0;
	
	if ( autoResolveFailed )
	{
		if ( autoSubmit )
		{
			printf( "conflicts! delete and continue?(y/n) " );
			gets_s( buf );

			if ( !strcmp( buf, "y" ) )
			{
				sprintf_s(
					buf, sizeof( buf ) - 1,
					"revert " );

				int index = 0;
				for ( const std::string& filePath : fileList )
				{
					if ( index++ > 0 )
						strcat_s( buf, sizeof( buf ) - 1, " " );

					strcat_s( buf, sizeof( buf ) - 1, filePath.c_str() );
				}

				p4( buf, nullptr );

				// delete
				sprintf_s(
					buf, sizeof( buf ) - 1,
					"change -d %d",
					newChangeListNo );
				p4( buf, nullptr );

				return 0;
			}
		}

		sprintf_s( buf, sizeof( buf ) - 1, "p4vc submit -c %d", newChangeListNo );
		system( buf );
		system( "pause" );
		return 0;
	}

	if ( !autoSubmit )
	{
		printf( "no conflicts! submit?(y/n) " );
		gets_s( buf );

		if ( strcmp( buf, "y" ) ) return 0;
	}

	if ( !autoResolveSuccessCount )
	{
		// delete
		sprintf_s(
			buf, sizeof( buf ) - 1,
			"change -d %d",
			newChangeListNo );
		p4( buf, nullptr );

		return 0;
	}

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"submit -c %d",
		newChangeListNo );
	p4( buf, nullptr );

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	perform merge
///
/// @param	name				name
/// @param	branchMap			map of branch
/// @param	accountIgnoreSet	accounts to ignore
///
/// @return	result code
////////////////////////////////////////////////////////////////////////////////////////////////////
int perform_merge(
	const std::string&      name,
	      BranchMap&        branchMap,
	const AccountIgnoreSet& accountIgnoreSet )
{
	printf( "revision : " );

	char buf[ 1024 * 100 ];
	gets_s( buf );

	int  revision;
	bool testMode = false;
	if ( !strstr( buf, "/test" ) )
	{
		revision = atoi( buf );
	}
	else
	{
		revision = atoi( strtok( buf, "/ " ) );
		testMode = true;
	}

	printf( "target branch(def: JP_Dev) : " );
	gets_s( buf );

	std::string dstBranch;
	if ( *buf )
		dstBranch = buf;
	else
		dstBranch = "JP_Dev";

	return perform_merge( name, branchMap, accountIgnoreSet, revision, 0, "", dstBranch, "", false, testMode, false );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	read revision info map
///
/// @param	branchFolder	folder name of target branch
/// @param	firstRevision	first revision number
/// @param	subFolderList	sub folder list
/// @param	revisionMap		revision map
///
/// @return	result code
////////////////////////////////////////////////////////////////////////////////////////////////////
int read_revision_map(
	const std::string&                   branchFolder,
	      int                            firstRevision,
	const std::list< std::string >&      subFolderList,
	      std::map< int, RevisionInfo >& revisionMap )
{
	enum class ReadMode
	{
		Normal,  ///< 일반
		Comment, ///< 주석
	};

	char buf[ 1024 ];
	for ( const std::string& subFolder : subFolderList )
	{
		sprintf_s(
			buf, sizeof( buf ) - 1,
			"changes -t -l \"//depot/%s/%s...@>=%d\"",
			branchFolder.c_str(),
			subFolder.c_str(),
			firstRevision );

		FILE* logFile = p4( buf, "log.txt" );
		ENSURE( logFile, return 1 );

		ReadMode     readMode = ReadMode::Normal;
		RevisionInfo curRevision;

		while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
		{
			switch ( readMode )
			{
			case ReadMode::Normal:
				{
					char* token = strtok( buf, " " );
					ENSURE( !strcmp( token, "Change" ), return 1 );

					token = strtok( nullptr, " " );
					curRevision.num = atoi( token );
					token = strtok( nullptr, " " );
					token = strtok( nullptr, " " );
					curRevision.date = token;
					ENSURE( fgets( buf, sizeof( buf ) - 1, logFile ), return 1 );
					readMode = ReadMode::Comment;
					curRevision.comment = "";
				}
				break;
			case ReadMode::Comment:
				{
					if ( *buf != '\t' )
					{
						readMode = ReadMode::Normal;
						revisionMap[ curRevision.num ] = curRevision;
						break;
					}

					curRevision.comment += buf;
				}
				break;
			}
		}

		fclose( logFile );
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	perform bulk merge
///
/// @param	name				name
/// @param	branchMap			map of branch
/// @param	accountIgnoreSet	accounts to ignore
///
/// @return	result code
////////////////////////////////////////////////////////////////////////////////////////////////////
int perform_bulk_merge(
	const std::string&      name,
	      BranchMap&        branchMap,
	const AccountIgnoreSet& accountIgnoreSet )
{
	printf( "first revision : " );

	char buf[ 1024 * 100 ];
	gets_s( buf );

	int  firstRevision;
	bool testMode = false;
	if ( !strstr( buf, "/test" ) )
	{
		firstRevision = atoi( buf );
	}
	else
	{
		firstRevision = atoi( strtok( buf, "/ " ) );
		testMode = true;
	}

	printf( "branch mapping : " );
	gets_s( buf );

	std::string branchMapping = buf;

	char* p = strtok( buf, "<" );
	ENSURE( p, return 1 );

	std::string srcBranch = p;

	p = strtok( nullptr, "=>" );
	ENSURE( p, return 1 );

	std::string dstBranch = p;

	printf( "branch mapping reverse : " );
	gets_s( buf );

	bool reverse = atoi( buf ) != 0;
	if ( reverse )
		std::swap( srcBranch, dstBranch );

	printf( "sub folder : " );
	gets_s( buf );

	std::list< std::string > subFolderList;
	p = strtok( buf, ", " );
	ENSURE( p, return  1 );
	
	do 
	{
		subFolderList.push_back( p );

	} while ( p = strtok( nullptr, ", " ) );

	auto iter = branchMap.find( srcBranch );
	if ( iter == branchMap.end() )
	{
		printf( "invalid branch.\n" );
		return 1;
	}

	const std::string& branchFolder = iter->second;

	std::map< int, RevisionInfo > revisionMap;

	int result = read_revision_map( branchFolder, firstRevision, subFolderList, revisionMap );
	if ( result ) return result;

	for ( auto& pair : revisionMap )
	{
		const RevisionInfo& revisionInfo = pair.second;
		int result = perform_merge(
			name, branchMap, accountIgnoreSet, revisionInfo.num, 0,
			srcBranch, dstBranch, branchMapping, reverse,
			testMode, true );
		if ( result ) return result;
	}

	printf( "OK!\n" );

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	perform merge job
///
/// @param	name				name
/// @param	branchMap			map of branch
/// @param	accountIgnoreSet	accounts to ignore
///
/// @return	result code
////////////////////////////////////////////////////////////////////////////////////////////////////
int perform_merge_job(
	const std::string&      name,
	      BranchMap&        branchMap,
	const AccountIgnoreSet& accountIgnoreSet )
{
	printf( "job name : " );

	char buf[ 1024 * 100 ];
	gets_s( buf );

	std::string jobName = buf;

	printf( "target branch : " );
	gets_s( buf );

	std::string dstBranch = buf;

	auto iter = branchMap.find( dstBranch );
	if ( iter == branchMap.end() )
	{
		printf( "invalid target branch.\n" );
		return 1;
	}

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"Change: new\r\n\r\n"
		"Description:\r\n"
		"\t%s 머징\r\n",
		jobName.c_str() );

	FILE* inputFile = fopen( "input.txt", "w" );
	fputs( utf8Mode ? ansi_to_utf8( buf ).c_str() : buf, inputFile );
	fclose( inputFile );

	FILE* logFile = p4( "change -i < input.txt" );
	ENSURE( logFile, return 1 );
	fgets( buf, sizeof( buf ) - 1, logFile );
	char* p = buf;
	while ( *p != ' ' ) ++p;
	++p;

	int newChangeListNum = atoi( p );
	printf( "newChangeList: %d\n", newChangeListNum );
	fclose( logFile );

	bool oldUtf8Mode = utf8Mode;
	utf8Mode = false;
	sprintf_s(
		buf, sizeof( buf ) - 1,
		"fixes -j %s",
		utf8Mode ? ansi_to_utf8( jobName ).c_str() : jobName.c_str() );

	logFile = p4( buf, "log.txt" );
	ENSURE( logFile, return 1 );

	utf8Mode = oldUtf8Mode;

	std::set< int > revisionNumSet;
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		const char* token = "fixed by change ";
		char* revisionStart = strstr( buf, token );
		if ( !revisionStart ) continue;

		revisionStart += strlen( token );
		char* revisionStr = strtok( revisionStart, " " );
		ENSURE( revisionStr, continue );

		revisionNumSet.insert( atoi( revisionStr ) );
	}

	for ( int revisionNum : revisionNumSet )
	{
		int result = perform_merge(
			name, branchMap, accountIgnoreSet, revisionNum, newChangeListNum,
			"", dstBranch, "", false, false, true );
		if ( result ) return result;
	}

	printf( "OK!\n" );

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	main function
///
/// @return	exit code
////////////////////////////////////////////////////////////////////////////////////////////////////
int main()
{
	std::string      name;
	BranchMap        branchMap;
	AccountIgnoreSet accountIgnoreSet;

	if ( !read_config( name, branchMap, accountIgnoreSet ) ) return 1;

	char buf[ 1024 * 100 ];
	printf( "mode(1: search, 2: merge, 3: bulk merge, 4: merge job, 5: search&merge, 6: revision) : " );
	gets_s( buf );
	Mode mode = (Mode)( atoi( buf ) - 1 );
	switch ( mode )
	{
	case Mode::Search:
	case Mode::SearchAndMerge:
		{
			search();
			if ( mode == Mode::Search )
			{
				system( "pause" );
				break;
			}
		}
		break;
	case Mode::Merge:
		{
			while ( true )
				perform_merge( name, branchMap, accountIgnoreSet );
		}
		break;
	case Mode::BulkMerge:
		{
			perform_bulk_merge( name, branchMap, accountIgnoreSet );
		}
		break;
	case Mode::MergeJob:
		{
			perform_merge_job( name, branchMap, accountIgnoreSet );
		}
		break;
	case Mode::Revision:
		{
			view_revision();
		}
		break;
	}

	system( "pause" );

	return 0;
}
