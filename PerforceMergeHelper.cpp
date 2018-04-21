// IntegrateHelper.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <map>
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
int         mode;

/// BranchMap typedef
typedef std::unordered_map< std::string, std::string > BranchMap;


enum class Mode
{
	Search,
	Merge,
	BulkMerge,
	SearchAndMerge,
	Revision,
	Max
};

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
	char buf[ 256 ];
	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C cp949 -c %s -p %s:%d -u %s -P %s branches > log.txt",
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

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	reads config
///
/// @param	name		name of user
/// @param	branchMap	map of branches
///
/// @return	success or failure
////////////////////////////////////////////////////////////////////////////////////////////////////
bool read_config( std::string& name, std::unordered_map< std::string, std::string >& branchMap )
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
	}

	fclose( configFile );

	return true;
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

	sprintf_s( buf, sizeof( buf ) - 1, "p4 -C cp949 changes -s submitted -l -m 3000 //depot/%s... > log.txt", folder.c_str() );
	system( buf );

	FILE* logFile = fopen( "log.txt", "r" );
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
		"p4 -C cp949 -c %s -p %s:%d -u %s -P %s integrate %s-n -b \"%s\" -s //depot/%s/...@%d,@%d",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str(),
		reverse ? "-r " : "",
		branchMapping.c_str(),
		branchMap[ srcBranch ].c_str(),
		revision, revision );

	system( buf );
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
	while ( *p != '[' ) p++;
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
		srcBranch  = "Trunk";
		return;
	}

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
/// @param	name		name
/// @param	branchMap	map of branch
/// @param	revision	number of revision
/// @param	dstBranch	target branch
/// @param	testMode	whether or not, it's test mode
/// @param	autoSubmit	whether or not, automatically submit if there is no conflict
///
/// @return	result code
////////////////////////////////////////////////////////////////////////////////////////////////////
int perform_merge(
	const std::string& name,
	      BranchMap&   branchMap,
	      int          revision,
	const std::string& dstBranch,
	      bool         testMode,
	      bool         autoSubmit )
{
	std::string srcBranch = "None";

	std::string personName;

	char buf[ 1024 * 100 ];
	sprintf_s( buf, sizeof( buf ) - 1, "p4 -C cp949 describe -s %d > log.txt", revision );
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
	std::list< std::string > commentList;
	char        branchInfo[ 256 ] = "";
	std::string readBranch = "None";
	std::string tag;

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
			personName = parse_submitter_name( p );
			parse_branch_name( buf, p, readBranch, srcBranch );

			parse_tag( buf, p, tag );

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

	printf( "old_comment : %s\n", comment.c_str() );
	printf( "person      : %s\n", personName.c_str() );
	printf( "src_branch  : %s\n", srcBranch.c_str() );
	printf( "dst_branch  : %s\n", dstBranch.c_str() );
	
	bool        reverse;
	std::string branchMapping = GetBranchMapping( srcBranch, dstBranch, reverse );

	if ( testMode )
	{
		test_integration( reverse, branchMapping, branchMap, srcBranch, revision );
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
		name.c_str(),
		srcBranch.c_str(), dstBranch.c_str(), tagPart.c_str(),
		revision, comment.c_str() );
	std::string newComment = buf;
	printf( "new_comment\n%s\n", newComment.c_str() );

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"%s\r\n"
		"Change: new\r\n\r\n"
		"Description:\r\n"
		"\t[%s][%s => %s]%s\r\n"
		"\t- @%d %s",
		srcComment.c_str(),
		name.c_str(),
		srcBranch.c_str(), dstBranch.c_str(), tagPart.c_str(),
		revision, personName != name ? ("[" + personName + "]").c_str() : "" );

	int lineIndex = 0;
	for ( const std::string& commentLine : commentList )
	{
		if ( lineIndex++ > 0 )
			strcat_s( buf, sizeof( buf ) - 1, "\r\n\t" );

		strcat_s( buf, sizeof( buf ) - 1, commentLine.c_str() );
	}

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

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C cp949 -c %s -p %s:%d -u %s -P %s integrate -c %d %s-b \"%s\" -s //depot/%s/...@%d,@%d",
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
		"p4 -C cp949 -c %s -p %s:%d -u %s -P %s change -o %d > log.txt",
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

	int autoResolveSuccessCount = 0;
	int autoResolveFailedCount  = 0;
	while ( fgets( buf, sizeof( buf ) - 1, logFile ) )
	{
		char* p = strtok( buf, "\t\r\n" );
		if ( !p ) continue;

		char command[ 256 ];
		sprintf_s(
			command, sizeof( command ) - 1,
			"p4 -C cp949 -c %s -p %s:%d -u %s -P %s resolve -o -am %s > log_resolve.txt",
			perforceWorkspace.c_str(),
			perforceHost.c_str(),
			perforcePort,
			perforceUserId.c_str(),
			perforceUserPw.c_str(),
			p );
		system( command );

		FILE* reolveLogFile = fopen( "log_resolve.txt", "r" );
		ENSURE( reolveLogFile, return 1 );
		bool autoResolved = true;
		if ( fgets( buf, sizeof( buf ) - 1, reolveLogFile ) )
		{
			ENSURE( fgets( buf, sizeof( buf ) - 1, reolveLogFile ), return 1 );
			autoResolved = (strstr( buf, "+ 0 conflicting" ) != nullptr);
		}
		printf( "autoResolved: %s-%s\n", p, autoResolved ? "success" : "failure" );
		fclose( reolveLogFile );

		if ( !autoResolved )
			autoResolveFailedCount++;
		else
			autoResolveSuccessCount++;
	}

	fclose( logFile );
	
	if ( autoResolveFailedCount )
	{
		sprintf_s( buf, sizeof( buf ) - 1, "p4vc submit -c %d", newChangeListNo );
		system( buf );
		system( "pause" );
		return 0;
	}

	if ( !autoSubmit )
	{
		printf( "no conflicts! summit?(y/n) " );
		gets_s( buf );

		if ( strcmp( buf, "y" ) ) return 0;
	}

	if ( !autoResolveSuccessCount )
	{
		// 삭제
		sprintf_s(
			buf, sizeof( buf ) - 1,
			"p4 -C cp949 -c %s -p %s:%d -u %s -P %s change -d %d",
			perforceWorkspace.c_str(),
			perforceHost.c_str(),
			perforcePort,
			perforceUserId.c_str(),
			perforceUserPw.c_str(),
			newChangeListNo );
		system( buf );

		return 0;
	}

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C cp949 -c %s -p %s:%d -u %s -P %s submit -c %d",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str(),
		newChangeListNo );
	system( buf );

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	perform merge
///
/// @param	name		name
/// @param	branchMap	map of branch
///
/// @return	result code
////////////////////////////////////////////////////////////////////////////////////////////////////
int perform_merge( const std::string& name, BranchMap& branchMap )
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

	return perform_merge( name, branchMap, revision, dstBranch, testMode, false );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief	perform bulk merge
///
/// @param	name		name
/// @param	branchMap	map of branch
///
/// @return	result code
////////////////////////////////////////////////////////////////////////////////////////////////////
int perform_bulk_merge( const std::string& name, BranchMap& branchMap )
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

	printf( "source branch : " );
	gets_s( buf );

	std::string srcBranch = buf;

	printf( "target branch : " );
	gets_s( buf );

	std::string dstBranch = buf;

	printf( "sub folder : " );
	gets_s( buf );

	std::string subFolder = buf;

	auto iter = branchMap.find( srcBranch );
	if ( iter == branchMap.end() )
	{
		printf( "invalid branch.\n" );
		return 1;
	}

	const std::string& branchFolder = iter->second;

	sprintf_s(
		buf, sizeof( buf ) - 1,
		"p4 -C cp949 -c %s -p %s:%d -u %s -P %s changes -t -l \"//depot/%s/%s...@>=%d\" > log.txt",
		perforceWorkspace.c_str(),
		perforceHost.c_str(),
		perforcePort,
		perforceUserId.c_str(),
		perforceUserPw.c_str(),
		branchFolder.c_str(),
		subFolder.c_str(),
		firstRevision );
	system( buf );

	FILE* logFile = fopen( "log.txt", "r" );
	ENSURE( logFile, return 1 );

	class RevisionInfo
	{
	public:
		int                      num;
		std::string              date;
		std::string              comment;
		std::list< std::string > fileList;
	};

	enum class ReadMode
	{
		Normal,  ///< 일반
		Comment, ///< 주석
	};

	ReadMode     readMode = ReadMode::Normal;
	RevisionInfo curRevision;

	std::list< RevisionInfo > revisionList;

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
				revisionList.push_back( curRevision );
				break;
			}

			curRevision.comment += buf;
		}
		break;
		}
	}

	fclose( logFile );

	for ( auto iter = revisionList.rbegin(); iter != revisionList.rend(); iter++ )
	{
		const RevisionInfo& revisionInfo = *iter;
		int result = perform_merge( name, branchMap, revisionInfo.num, dstBranch, testMode, true );
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
	std::string name;
	BranchMap   branchMap;

	if ( !read_config( name, branchMap ) ) return 1;

	char buf[ 1024 * 100 ];
	printf( "mode(1: search, 2: merge, 3: bulk merge, 4: search&merge, 5: revision) : " );
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
				perform_merge( name, branchMap );
		}
		break;
	case Mode::BulkMerge:
		{
			perform_bulk_merge( name, branchMap );
		}
		break;
	case Mode::Revision:
		{
			view_revision();
		}
		break;
	}

    return 0;
}
