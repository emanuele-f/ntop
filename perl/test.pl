#
# (C) 2008 - Luca Deri <deri@ntop.org>
#

#use strict;
#use warnings;

send_http_header(1, "Known ntop hosts");

# ---------------------------------------------

loadHosts();

#while ( my ($key, $value) = each(%hosts) ) {
#    print "$key => $value\n";
#}

#my @sorted_by_hostname      = sort { ${$a}{'ipAddress'}  cmp ${$b}{'ipAddress'} }  values %hosts;
#my @traffic = sort keys %hosts;

print "################################################\n";

foreach $key (sort(keys %hosts)) {
    print "[key=", $key,"][ipAddress=",${$key}{'ipAddress'},"][macAddress=",${$key}{'macAddress'},"][pktSent/pktRcvd=",${$key}{'pktSent'},"/",${$key}{'pktRcvd'},"]\n";
}

my $h = "luca";

print $a,"<-\n";
print $a{'bytesRcvd'},"<-\n";

{
    my %t = ${$h};
    foreach $key (sort(keys %t)) {
	print $key,"=",$t{$key},"\n";
    }
}

     
#for my $k1 ( sort keys %hosts ) {
#    print "k1: $k1\n";
#    for my $k2 ( keys %{$k1} ) {
#	print "k2: $k2 $hosts{ $k1 }{ $k2 }\n";
#    }
#}

#foreach $key (sort(keys %hosts{'a'})) {
#    print $key,"\n";
#}

exit
# ---------------------------------------------

getFirstHost(0);
loadHost();

sendString("<center>\n");
sendString("<table border>\n");
sendString("<tr><th>MAC</th><th colspan=2>IP</th><th>Packets</th><th>Bytes</th></tr>\n");


while(($host{'ethAddress'} ne "")
      || ($host{'ipAddress'} ne ""))  {
    my $mac_addr;

    if($host{'ethAddress'} ne "") {
	my $mac = $host{'ethAddress'};
	$mac =~ tr/:/_/;
	$mac_addr = "<A HREF=/".$mac.".html>".$host{'ethAddress'}."</A>";
    } else {
	$mac_addr = "";
    }

    sendString("<tr><td>".$mac_addr
	       ."&nbsp;</td><td><A HREF=/".$host{'ipAddress'}.".html>".$host{'ipAddress'}."</A>"
	       ."&nbsp;</td><td>".$host{'hostResolvedName'}
	       ."&nbsp;</td><td> ".$host{'pktSent'}." / ".$host{'pktRcvd'}.""
	       ."&nbsp;</td><td> ".$host{'bytesSent'}." / ".$host{'bytesRcvd'}.""
	       ."&nbsp;</td></tr>\n");
    getNextHost(0);
    loadHost();
}

sendString("</table>\n");
sendString("</center>\n");
send_html_footer();

########

