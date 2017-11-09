/* complile with make */
/* see included Makefile */
/* note public string */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

/* hardcoding thresholds for nagios exit codes */
#define CRIT_HT 110000
#define CRIT_LT 5000
#define WARN_HT 90000
#define WARN_LT 10000

#define OK 0
#define WARNING 1
#define CRITICAL 2
#define UNKNOWN 3

int
main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage %s <hostname>\n", argv[0]);
		exit(UNKNOWN);
	}

	/* init */
	struct snmp_session session, *ss;
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;

	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;

	struct variable_list *vars;
	int status, ec = 0;
	long retval;
	float div_val;
	const char *power_oid = ".1.3.6.1.4.1.13891.101.4.4.1.4.1";
	const char *cstr = "public";

	init_snmp("snmpapp");
	snmp_sess_init(&session);
	session.peername = strndup(argv[1], strlen(argv[1]));
	/* not using v3 */
	session.version = SNMP_VERSION_1;
	session.community = (u_char *)strndup(cstr, strlen(cstr));
	session.community_len = strlen((char *)session.community);
	session.timeout = 500000;
	session.retries = 2;

	ss = snmp_open(&session);
	if (!ss) {
		snmp_perror("snmp_open");
		snmp_log(LOG_ERR, "could not open session\n");
		exit(UNKNOWN);
	}

	pdu = snmp_pdu_create(SNMP_MSG_GET);
	read_objid(power_oid, anOID, &anOID_len);
	/* must pair with null var */
	snmp_add_null_var(pdu, anOID, anOID_len);

	status = snmp_synch_response(ss, pdu, &response);
	if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
		/* should only be one response so check that */
		vars = response->variables;
		if (vars->type != ASN_INTEGER) {
			printf("returned unexpected type\n");
			ec = UNKNOWN;
			goto cleanup;
		}
		retval = *(vars->val.integer);
	} else {
		if (status == STAT_TIMEOUT) {
			printf("check_ups_power response timeout\n");
		} else
		if (status == STAT_SUCCESS)
			fprintf(stderr, "Error in packet\nReason: %s\n", 
					snmp_errstring(response->errstat));
		else
			snmp_sess_perror("snmpget", ss);

		ec = UNKNOWN;
		goto cleanup;
	}

	/* hardcode for now */
	div_val = (float)retval / 1000.00;
	/* check thresholds and print perf data */
	if (retval < CRIT_LT || retval > CRIT_HT) {
		printf("UPS POWER CRITICAL - Power(Kw) %.2f | 'power(Kw)'=%.2f;%d;%d;%d;%d\n",
			div_val, div_val, WARN_HT, CRIT_HT, CRIT_LT, CRIT_HT);	
		ec = CRITICAL;
	} else
	if (retval < WARN_LT || retval > WARN_HT) {
		printf("UPS POWER WARNING - Power(Kw) %.2f | 'power(Kw)'=%.2f;%d;%d;%d;%d\n",
			div_val, div_val, WARN_HT, CRIT_HT, CRIT_LT, CRIT_HT);	
		ec = WARNING;
	} else {
		printf("UPS POWER OK - Power(Kw) %.2f | 'power(Kw)'=%.2f;%d;%d;%d;%d\n",
			div_val, div_val, WARN_HT, CRIT_HT, CRIT_LT, CRIT_HT);	
		ec = OK;
	}

cleanup:
	if (response)
		snmp_free_pdu(response);
	snmp_close(ss);
	/* exit code for Nagios */
	exit(ec);
}

