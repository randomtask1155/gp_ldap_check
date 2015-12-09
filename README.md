#Purpose
The purpose of this tool it so help database administrators quickly generate and test pg_hba.conf ldap configuration settings

GPDB uses the c based ldap api to authenticate database users. The authentication methods used in GPDB are exactly the same as the latest postgresql. The problem most customers face is the ldapsearch utility that gets installed with most RHEL distrobutions does not use the same method calls when querying ldap. So we see situation where ldapsearch will work successfully but GPDB ldap auth will fail. This tool is designed to solve that problem.

#Build
```
$> git clone https://github.com/username.../gp_ldap_check.git
$> cd gp_ldap_check
$> make
$> ./gp_ldap_check -h
```

#Usage
```
Example usage:
./gpldap_tool -g -s ldap://server.domain.com -b "cn=Users,dc=saturn,dc=local" -d "cn=myuser,cn=Users,dc=saturn,dc=local" -w changeme -f "(cn=someuser)"


This tool is designed to validate ldap connectivity and return suggested
ldap configuration parameters for pg_hba.conf

Behvior Options:
	-g	Behave like GPDB ( DEFAULT )
		When TLS is enabled GPDB will first send a start
		TLS request to the LDAP server.  Port 389 is
		the preferred port in most implementations. Port 636
		is reserved for TLS only and will reject any start
		TLS requests because it is expecting the TLS handshake
		without the start request.

		port 389 + NoTLS
		   - GPDB will send unencrypted traffic
		port 636 + NoTLS
		   - GPDB will send unencrypted traffic
		port 389 + StartTLS
		   - GPDB will send start TLS request and encrypt traffic
		port 636 + StartTLS
		   - GPDB will send start TLS request and encrypt traffic

	-l	Behave like ldapsearch utility
		ldapsearch utility can be installed via "yum install openldap-clients" and supports passing
		in the URI for the ldap server.

		port 389 + NoTLS
		   - ldapsearch will send unencrypted traffic
		port 636 + NoTLS
		   - ldapsearch will send the TLS client hello encrypting traffic
		port 389 + StartTLS
		   - ldapsearch will send start TLS request and encrypt traffic
		port 636 + StartTLS
		   - ldapsearch will ignore startTLS switch and immediately
		     send the TLS client hello encrypting traffic
Binding Options:
	-s	(Required) hostname or IP of ldap server

	-b	(Required) Specify the Basedn
		Example: cn=Users,dc=domain,dc=com

	-d	(Required) User that will bind with ldap
		The user must be the full distinguished name like "cn=user,ou=ASIA,dc=domain,dc=com"

	-t	(Optional) Send StartTLS request

	-p	(Optional) Server port default 389 for ldap and 636 for ldaps
		Overrride for ldap URI

	-w	(Optional) User password for binddn user

	-f	(Optional) Ldap Search filter
		Specify a user account you want to search for with "(cn=user)"

	-?	This help
```

#Example 1
Here we test if user "someuser" can be found in ldap base directory "cn=Users,dc=saturn,dc=local" using username/password as "myuser/changeme".  if the command executes successfully then pg_hba.conf configuration settings will be generated automatically

```
[root@pccadmin gp_ldap_check]# ./gp_ldap_check -g -s 172.28.18.60 -b "cn=Users,dc=support,dc=pivotal" -d "cn=danl,cn=Users,dc=support,dc=pivotal" -w Chang3m3 -f "(cn=d*)"

#-Connecting to host 172.28.18.60 at port 389...
#-LDAP Bind was successful!
#-Searching cn=Users,dc=support,dc=pivotal using filter (cn=d*)
#-Dumping Results:
Found dn: CN=Domain Computers,CN=Users,DC=support,DC=pivotal
Found dn: CN=Domain Controllers,CN=Users,DC=support,DC=pivotal
Found dn: CN=Domain Admins,CN=Users,DC=support,DC=pivotal
Found dn: CN=Domain Users,CN=Users,DC=support,DC=pivotal
Found dn: CN=Domain Guests,CN=Users,DC=support,DC=pivotal
Found dn: CN=Denied RODC Password Replication Group,CN=Users,DC=support,DC=pivotal
Found dn: CN=DnsAdmins,CN=Users,DC=support,DC=pivotal
Found dn: CN=DnsUpdateProxy,CN=Users,DC=support,DC=pivotal
Found dn: CN=danl,CN=Users,DC=support,DC=pivotal
#-Disconnecting from LDAP
#~Outputing pg_hba.conf config settings:

ldap ldapserver="172.28.18.60" ldapport="389" ldapsearchattribute="cn=" ldapbasedn="cn=Users,dc=support,dc=pivotal" ldapbinddn="cn=danl,cn=Users,dc=support,dc=pivotal" ldapbindpasswd="<Enter Password Here>"
```

#Example 2
Test a single user bind request

```
[root@pccadmin gp_ldap_check]# ./gp_ldap_check -g -s 172.28.18.60 -b "cn=Users,dc=support,dc=pivotal" -d "cn=danl,cn=Users,dc=support,dc=pivotal"
Password:

#-Connecting to host 172.28.18.60 at port 389...
#-LDAP Bind was successful!
#-Disconnecting from LDAP
#~Outputing pg_hba.conf config settings:

ldap ldapserver="172.28.18.60" ldapport="389" ldapprefix="cn=" ldapsuffix=",cn=Users,dc=support,dc=pivotal"
```
