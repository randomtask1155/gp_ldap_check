#include <stdio.h>
#include <ldap.h>
#include <stdlib.h>
#include <unistd.h>

#define SCOPE LDAP_SCOPE_SUBTREE

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
"\t-s\t(Required) hostname or IP of ldap server\n"
"\n\t-b\t(Required) Specify the Basedn\n"
"\t\tExample: cn=Users,dc=domain,dc=com\n"
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


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/* Parse out the prefix string
*      Currently GPDB only supports the following filter format so we will too
*      filter = palloc(strlen(attributes[0]) + strlen(port->user_name) + 4);
*      sprintf(filter, "(%s=%s)", attributes[0],port->user_name);
*/
void getPrefix(char *filter, char *dest) {
	char FilterPrefix[256];
	int prefixPosition = 0;
	size_t i;
	for (i = 0; i < strlen(filter) && filter[i] != '\0'; i++) {
		if ( filter[i] != 40 ) { // exclude (
			if ( filter[i] == 61 ) { // if we reached the = sign
				FilterPrefix[prefixPosition] = '\0';
				break;
			}
			FilterPrefix[prefixPosition] = filter[i];
			prefixPosition += 1;
		}
	}
	strcpy(dest, FilterPrefix);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
typedef struct {
        char *hostname;
        int port;
        char *filter;
        char prefix[256];
        char *binddn;
        char *basedn;
        int tls;
} ldapconfig;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/* Terminate the char arrah with nulls */
void nullChar( char *c ) {
	int i = 0;
	for ( i = 0; i < strlen(c); i ++ ) {
		c[i] = '\0';
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/* Build the config strings based on the configuration */
void dumpConfig(ldapconfig *ldcfg) {
	char output[2048];
	char printbuf[2048];

	nullChar(printbuf);
	nullChar(output);

	strcat(output, "ldap ");
	if (ldcfg->tls) {
		strcat(output, "ldaptls=1 ");
	}
	sprintf(printbuf, "ldapserver=\"%s\" ldapport=\"%d\" ", ldcfg->hostname, ldcfg->port);
	strcat(output, printbuf);

	// if there is a filter then print the search version otherwise print the bind version
	if (ldcfg->filter != NULL ) {
		sprintf(printbuf, "ldapsearchattribute=\"%s\" ldapbasedn=\"%s\" ldapbinddn=\"%s\" ldapbindpasswd=\"<Enter Password Here>\"", ldcfg->prefix, ldcfg->basedn, ldcfg->binddn);
		strcat(output, printbuf);
	} else {
		sprintf(printbuf, "ldapprefix=\"%s\" ldapsuffix=\",%s\"", ldcfg->prefix, ldcfg->basedn);
		strcat(output, printbuf);
	}
	printf("#~Outputing pg_hba.conf config settings:\n\n%s\n", output);	
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int main( int argc, char **argv )
{
	LDAP *ld;
	LDAPMessage *result, *e;
	char *dn;
	int version, rc;
	int opt;
	extern char *optarg;
	ldapconfig *ldcfg = malloc(sizeof(ldapconfig));

	// Seupt the default LDAP params
	int PORTNUMBER = 0;
	int START_TLS = 0;
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
			case 'l':
				LDAPSEARCH = 1;
				break;
			case 's':
				URI = strdup(optarg);
				break;
			case 't':
				START_TLS = 1;
				break;
			case 'p':
				PORTNUMBER = atoi(optarg);
				break;
			case 'b':
				BASEDN = strdup(optarg);
				break;
			case 'd':
				BIND_DN = strdup(optarg);
				break;
			case 'w':
				BIND_PW = strdup(optarg);
				break;
			case 'f':
				FILTER = strdup(optarg);
				break;
			case '?':
				usage();
				break;
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

	if ( !LDAPSEARCH && !GPDB ){GPDB = 1; } // default to GPDB mode

	if ( BIND_PW == NULL )
	{
		BIND_PW = getpass("Password: ");
	}
	
	if ( !PORTNUMBER ) { PORTNUMBER = 389; }

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	/* now we have a good config populate the ldcfg struct */
	ldcfg->hostname = URI;
	ldcfg->port = (int)PORTNUMBER;
	ldcfg->basedn = BASEDN;
	ldcfg->binddn = BIND_DN;
	ldcfg->tls = (int)START_TLS;
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	/* init the LDAP object */
	if ( (ld = ldap_init( ldcfg->hostname, ldcfg->port )) == NULL ) 
	{
		printf( "ldap_init Failed\n" );
		exit(3);
	}
	/* Print out an informational message. */
	printf( "\n#-Connecting to host %s at port %d...\n", ldcfg->hostname, ldcfg->port );


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
		ldcfg->filter = FILTER;
		getPrefix(FILTER, ldcfg->prefix);
		

		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		/* Perform the LDAP operations. In this example, a simple search operation is performed. The client iterates through each of the entries returned and prints out the DN of each entry. */
		printf("#-Searching %s using filter %s\n", ldcfg->basedn, ldcfg->filter );
		rc = ldap_search_ext_s( ld, ldcfg->basedn, SCOPE, ldcfg->filter, NULL, 0, NULL, NULL, NULL, 0, &result );
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
	} else {
		// no filter was defined but user did specify a binddn so filter on that.
		getPrefix(ldcfg->binddn, ldcfg->prefix);
	}

	/* Disconnect from the server. */
	printf("#-Disconnecting from LDAP\n");
	ldap_unbind( ld );
	
	dumpConfig(ldcfg);
	return( 0 );
}
