#include <stdio.h>
#include <ldap.h>
#include <stdlib.h>
#include <unistd.h>

/* Specify the search criteria here. */
//#define HOSTNAME "win-t66lm4e0ujj.saturn.local"
//#define PORTNUMBER 636
//#define BASEDN "cn=Users,dc=saturn,dc=local"
//#define BIND_DN "cn=danl,cn=Users,dc=saturn,dc=local"
//#define BIND_PW "changeme"
//#define URL "ldaps://win-t66lm4e0ujj.saturn.local"
//#define FILTER "(cn=danl)"
#define SCOPE LDAP_SCOPE_SUBTREE

void usage(){
	printf("\nExample usage:\n");

printf(

"./gpldap_tool -g -s ldap://server.domain.com -b \"cn=Users,dc=saturn,dc=local\" -d \"cn=myuser,cn=Users,dc=saturn,dc=local\" -w changeme -f \"(cn=someuser)\" \n\n"
"\nThis tool is designed to validate ldap connectivity and return suggested\n"
"ldap configuration parameters for pg_hba.conf\n\n"

"Behvior Options: \n"
		"\t-g\tBehave like GPDB ( DEFAULT )\n"
		"\t\tWhen TLS is enabled GPDB will first send a start\n"
		"\t\tTLS request to the LDAP server.  Port 389 is\n"
		"\t\tthe preferred port in most implementations. Port 636\n"
		"\t\tis reserved for TLS only and will reject any start\n"
		"\t\tTLS requests because it is expecting the TLS handshake\n"
		"\t\twithout the start request.\n\n"
		"\t\tport 389 + NoTLS\n"
		"\t\t   - GPDB will send unencrypted traffic\n"
		"\t\tport 636 + NoTLS\n"
		"\t\t   - GPDB will send unencrypted traffic\n"
		"\t\tport 389 + StartTLS\n"
		"\t\t   - GPDB will send start TLS request and encrypt traffic\n"
		"\t\tport 636 + StartTLS\n"
		"\t\t   - GPDB will send start TLS request and encrypt traffic\n\n"


		"\t-l\tBehave like ldapsearch utility\n"
		"\t\tldapsearch utility can be installed via \"yum install openldap-clients\" and supports passing\n"
		"\t\tin the URI for the ldap server.\n\n"
		"\t\tport 389 + NoTLS\n"
		"\t\t   - ldapsearch will send unencrypted traffic\n"
		"\t\tport 636 + NoTLS\n"
		"\t\t   - ldapsearch will send the TLS client hello encrypting traffic\n"
		"\t\tport 389 + StartTLS\n"
		"\t\t   - ldapsearch will send start TLS request and encrypt traffic\n"
		"\t\tport 636 + StartTLS\n"
		"\t\t   - ldapsearch will ignore startTLS switch and immediately\n"
		"\t\t     send the TLS client hello encrypting traffic\n"


"Binding Options:\n"
"\t-s\t(Required) URI for ldap host [ldap://|ldaps://]server.domain.com\n"
"\t\tldap://ldapserver.domain.com communicates over port 389\n"
"\t\tldaps://ldapserver.domain.com communicates over port 636\n"
"\n\t-b\t(Required) Search path we apply the filter to\n"
"\t\tSpecify \'(cn=username)\'  to search for a user within the BaseDN\n"
"\n\t-d\t(Required) User that will bind with ldap\n"
"\t\tThe user must be the full distinguished name like \"cn=user,ou=ASIA,dc=domain,dc=com\"\n"
"\n\t-t\t(Optional) Send StartTLS request\n"
"\n\t-p\t(Optional) Server port default 389 for ldap and 636 for ldaps\n"
"\t\tOverrride for ldap URI\n"
"\n\t-w\t(Optional) User password for binddn user\n"
"\n\t-f\t(Optional) Ldap Search filter\n"
"\t\tSpecify a user account you want to search for with \"(cn=user)\"\n"
"\n\t-?\tThis help\n\n");
	exit(2);

}

int main( int argc, char **argv )
{
	LDAP *ld;
	LDAPMessage *result, *e;
	char *dn;
	char *LDAP_PREFIX = "ldap://";
	char *LDAPS_PREFIX = "ldaps://";
	int version, rc;
	int opt;
	extern char *optarg;

	// Seupt the default LDAP params
	int PORTNUMBER = 0;
	int START_TLS = 0;
	char *HOSTNAME;
	char *BASEDN;
	char *BIND_DN;
	char *BIND_PW = NULL;
	char *URI;
	char *FILTER = NULL;

	// Define the control params
	int GPDB = 0; 
	int LDAPSEARCH = 0;


	/* Parse the args */
	if ( argc <= 1 ) {
		printf("No Arguments were passed in\n");
		usage();
	}
	while( ( opt = getopt( argc, argv, "glts:p:b:d:w:f:")) != -1 ) 
	{
        	switch( opt )
		{
			case 'g':
				GPDB = 1;
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case 'l':
				LDAPSEARCH = 1;
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case 's':
				URI = strdup(optarg);
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case 't':
				START_TLS = 1;
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case 'p':
				PORTNUMBER = atoi(optarg);
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case 'b':
				BASEDN = strdup(optarg);
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case 'd':
				BIND_DN = strdup(optarg);
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case 'w':
				BIND_PW = strdup(optarg);
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case 'f':
				FILTER = strdup(optarg);
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case '?':
				usage();
			break;
			//|~~~~~~~~~~~~~~~~~~~~~~~~~~~|
			case '*':
				printf("Invalid argument\n");
				usage();
			break;
		}
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Make sure the args are good
	if ( !URI || !BASEDN || !BIND_DN )
	{
		printf("\nMissing one or more required arguments\n");
		printf("-s, -b, and -d are required arguments\n");
		usage();
	}

	if ( !LDAPSEARCH && !GPDB )
	{
		// default to GPDB
		GPDB = 1;
	}

	if ( BIND_PW == NULL )
	{
		BIND_PW = getpass("Password: ");
	}
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// get the hostname from URI
	char *URI_PREFIX = malloc(strlen(URI));

	strncpy(URI_PREFIX, URI, 7);
	if ( strcmp(URI_PREFIX, LDAP_PREFIX ) == 0 ) 
	{
		if ( !PORTNUMBER ) { PORTNUMBER = 389; } // default for ldap://
		HOSTNAME = URI + 7;
	}

	strncpy(URI_PREFIX, URI, 8);
	if ( strcmp(URI_PREFIX, LDAPS_PREFIX ) == 0 ) 
	{
		if ( !PORTNUMBER ) { PORTNUMBER = 636; } // default for ldaps://
		HOSTNAME = URI + 8;
	}
	if ( HOSTNAME == NULL ) 
	{
		printf("The URI path: %s is invalid\n", URI);
		usage();
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	/* init the LDAP object */
	if ( (ld = ldap_init( HOSTNAME, PORTNUMBER )) == NULL ) 
	{
		printf( "ldap_init Failed\n" );
		exit(3);
	}
	/* Print out an informational message. */
	printf( "\n#-Connecting to host %s at port %d...\n", HOSTNAME, PORTNUMBER );


	/* Use the LDAP_OPT_PROTOCOL_VERSION session preference to specify that the client is an LDAPv3 client. */
	version = LDAP_VERSION3;
	ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &version );


	/*  
	// LDAPSEARCH BEHAVIOR:
	//
	// Take the URI and initialize the connection directly based on the URI path. ldap_initialize is smart enough to know if user is asking for SSL
	// or not based on the "ldaps://" ( ssl port 636 )  or "ldap://" ( default port 389 ) prefix
	//
	// We should not allow TLS to be enabled with port 636 because ldap_initialize handles all of that automatically.  
	// The user can test TLS with port 389 by setting TLS flag, URI path to ldap://xxx, and port to 389
	*/
	if ( LDAPSEARCH ) 
	{
		if ( START_TLS && PORTNUMBER != 636 )
		{
			printf("#-Starting TLS...\n");
			rc = ldap_start_tls_s( ld, NULL, NULL );
			if ( rc != LDAP_SUCCESS ) 
			{
				fprintf(stderr, "Staring TLS server failed:%d: %s\n", rc, ldap_err2string(rc) );
				exit(3);
			}
		} else if ( START_TLS && PORTNUMBER == 636 ) {
				printf("WARNING: Ignoring the TLS flag because LDAP port is set to 636 reserved for SSL+TLS.  This tool will handle the TLS session intialization automatically just like ldapsearch behavior\n");
		}
		rc = ldap_initialize( &ld, URI );
		if ( rc != LDAP_SUCCESS ) 
		{
			fprintf(stderr, "ldap initialize failed:%d: %s\n", rc, ldap_err2string(rc) );
			exit(3);
		}
	}

	/*
	// GPDB Behavior:
	//
	// GPDB  only supports TLS over port 389 because SSL support has been removed
	// Attempts to start TLS over port 636 will fail but we don't need to check for it
	*/
	if ( GPDB && START_TLS ) 
	{
		printf("#-Starting TLS...\n");
		rc = ldap_start_tls_s(ld, NULL, NULL);
		if ( rc != LDAP_SUCCESS ) 
		{	
			fprintf(stderr, "Staring TLS server failed:%d: %s\n", rc, ldap_err2string(rc) );
			exit(4);
		}
	}


	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	/* Bind to the server. */
	rc = ldap_simple_bind_s( ld, BIND_DN, BIND_PW );
	if ( rc != LDAP_SUCCESS ) {
		fprintf(stderr, "ldap_simple_bind_s:%d: %s\n", rc, ldap_err2string(rc));
		return( 1 );
	}
	printf( "#-LDAP Bind was successful!\n");

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    /* If the filter argument is set
	 * Perform the LDAP search operation. The client iterates
	 * through each of the entries returned and prints out the DN of each entry. */
	if ( FILTER != NULL )
	{
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		/* Perform the LDAP operations. In this example, a simple search operation is performed. The client iterates through each of the entries returned and prints out the DN of each entry. */
		printf("#-Searching %s using filter %s\n", BASEDN, FILTER );
		rc = ldap_search_ext_s( ld, BASEDN, SCOPE, FILTER, NULL, 0, NULL, NULL, NULL, 0, &result );
		if ( rc != LDAP_SUCCESS ) {
			fprintf(stderr, "ldap_search_ext_s:%d: %s\n", rc, ldap_err2string(rc));
			return( 1 );
		}

		printf("#-Dumping Results:\n");
		int total_entries = 0;
		for ( e = ldap_first_entry( ld, result ); e != NULL; e = ldap_next_entry( ld, e ) ) {
			if ( (dn = ldap_get_dn( ld, e )) != NULL ) {
				printf( "Found dn: %s\n", dn );
				++total_entries;
				ldap_memfree( dn );
			}
		}
		ldap_msgfree( result );

		if ( total_entries == 0 ){
			printf("No results found\n");
		}
	}

	/* Disconnect from the server. */
	printf("#-Disconnecting from LDAP\n\n");
	ldap_unbind( ld );


	/* Print out the suggested pg_hba.conf params */
	if ( GPDB ) {

		printf("\nGenerating possible pg_hba.conf ldap configurations...\n");

		/* TLS disabled
		 * Multiple Organizational Units
		 * Authenticate specific user with given BaseDN */
		printf("\nNo encryption:\n");
		/* host    all     all        0.0.0.0/0         ldap ldapserver=dc1.escops.local
		 *  ldapport=389 ldapsearchattribute=cn ldapbasedn="ou=other,dc=escops,dc=local"
		 *  ldapbinddn="cn=danl,cn=Users,dc=escops,dc=local" ldapbindpasswd="changeme" */
		printf("1. Search multiple organizational unit template ( Password left empty )\n");
		/* ldap ldapserver=win-t66lm4e0ujj.saturn.local ldapport=389 ldapprefix="cn="
		 * ldapsuffix=",cn=Users,dc=saturn,dc=local" */
		printf("ldap ldapserver=\"%s\" ldapport=\"%d\" ldapsearchattribute=\"cn\" ldapbasedn=\"%s\" ldapbinddn=\"%s\" ldapbindpasswd=\"<Enter Password Here>\"\n",
			HOSTNAME,
			PORTNUMBER,
			BASEDN,
			BIND_DN );
		printf("2. A user from the BaseDN\n");
		printf("ldap ldapserver=\"%s\" ldapport=\"%d\" ldapprefix=\"cn=\" ldapsuffix=\",%s\"\n",
			HOSTNAME,
			PORTNUMBER,
			BASEDN
			);

		// Now with TLS enabled
		printf("\nTLS Encryption enabled:\n");
		printf("1. Search multiple organizational unit template ( Password left empty )\n");
		printf("ldap ldapserver=\"%s\" ldapport=\"%d\" ldapsearchattribute=\"cn\" ldapbasedn=\"%s\" ldapbinddn=\"%s\" ldapbindpasswd=\"<Enter Password Here>\" ldaptls=1\n",
			HOSTNAME,
			PORTNUMBER,
			BASEDN,
			BIND_DN );
		printf("2. A user from the BaseDN\n");
		printf("ldap ldapserver=\"%s\" ldapport=\"%d\" ldapprefix=\"cn=\" ldapsuffix=\",%s\" ldaptls=1\n",
			HOSTNAME,
			PORTNUMBER,
			BASEDN
			);
	}
	return( 0 );
}
