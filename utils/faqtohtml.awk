# ntop - Burton M. Strauss III - Nov2002
BEGIN {
    print "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">"
    print "<html>"
    print "<head>"
    print "<!--meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" -->"
    print "<meta http-equiv=\"Content-Style-Type\" content=\"text/css\">"
    print "<meta http-equiv=\"Window-target\" content=\"_top\">"
    print "<meta name=\"description\" content=\"ntop (http://www.ntop.org) FAQ file.\">"
    print "<meta name=\"author\" content=\"ntop\">"

    while (getline < "version.c" > 0) {
        if($3 == "version") {
            gsub(/["; ]/, "", $5) 
            print "<meta name=\"generator\" content=\"ntop " $5 "\">"
        }
    }

    print "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">"
    print "	<title>ntop FAQ</title>"
    print "</head>"
    print "<body background=\"white_bg.gif\">"
    print "<h3>ntop FAQ...</h3>"
    print "<p>This is an unsophisticated, automated conversion of the source file, docs/FAQ into html."
    print "Please report problems to the ntop-dev mailing list."
    print "But remember, it's not about making it look good, it's about making the content available.</p>"
    print "<hr>"
    pastHeader="No"
    pclose="</p>"
    lines=0
    allDash=0
    allEquals=0
    header=0
    empty1=0
    empty2=0
    section=0
    q=0
    a=0
    text=0
    pre=0
    updated=0
    added=0
}

{
    lines++
    sub(/[\r\n]$/, "")
}

/^\-+$/ { 
    allDash++
    pastHeader="Yes"
    next
}

/^=+$/ { 
    allEquals++
    next
}

/^\-\-\-\-\-/ {
    gsub(/^\-*/, "", $0)
    gsub(/\-*$/, "", $0)
    print "<h2>" $0 "</h2>"
    next
}

pastHeader == "No" {
    header++
    next
}

$0 == "" {
    empty1++
    if (pclose != "") { print pclose }
    pclose=""
    next
}

/^ +$/ {
    empty2++
    if (pclose != "") { print pclose }
    pclose=""
    next
}

/^Section/ {
    section++
    if (pclose != "") { print pclose }
    pclose=""
    print "<h2>" $0 "</h2>"
    next
}

$1 == "(Added" {
    added++
    if (pclose != "") { print pclose }
    pclose=""
    print "<center><i>" $0 "</i></center>"
    next
}

$1 == "(Updated" {
    updated++
    if (pclose != "") { print pclose }
    pclose=""
    print "<center><i>" $0 "</i></center>"
    next
}

/^[Qq]\. / {
    q++
    if (pclose != "") { print pclose }
    print "<br>"
    print "<p>"
    pclose="</p>"
    print "<b>" $1 "</b>&nbsp;"
    $1 = ""
    print $0
    next
}

/^[Aa]\. / {
    a++
    if (pclose != "") { print pclose }
    print "<p>"
    pclose="</p>"
    print "<b>" $1 "</b>&nbsp;"
    $1 = ""
    print $0
    next
}

(substr($0, 1, 3) == "   " && substr($0, 4, 1) != " ") {
    text++
    if (pclose == "") {
        print "<p>"
        pclose="</p>"
    }
    print $0
    next
}

{ 
    pre++
    if (pclose != "</pre>") {
        print pclose
        print "<pre>"
        pclose="</pre>"
    }
    print $0
    next
}

END {
    if (pclose != "") { print pclose }
    printf("<!-- Total Lines........ %5d -->\n\n" \
           "<!-- (skipped) --s...... %5d -->\n" \
           "<!-- (skipped) ==s...... %5d -->\n" \
           "<!-- (skipped) header... %5d -->\n" \
           "<!-- empty (\"\")......... %5d -->\n" \
           "<!-- empty (//)......... %5d -->\n" \
           "<!-- Sections........... %5d -->\n" \
           "<!-- Q:................. %5d -->\n" \
           "<!-- A:................. %5d -->\n" \
           "<!-- Normal text........ %5d -->\n" \
           "<!-- Preformatted text.. %5d -->\n" \
           "<!-- Updated.xxmmmyyyy.. %5d -->\n" \
           "<!-- Added.xxmmmyyyy.... %5d -->\n" \
           "<!-- ...........TOTAL... %5d -->\n",
                lines,
                allDash,
                allEquals,
                header,
                empty1,
                empty2,
                section,
                q,
                a,
                text,
                pre,
                updated,
                added,
                (allDash + allEquals + header + empty1 + empty2 + section + q + a + text + pre + updated + added))
                
    print "</body>"
    print "</html>"
}

#Q. What is ntop?
#A. ntop is an open source network top - the official website can be found at
#   http://www.ntop.org/

