#!/bin/sh
#learned about mult choice through https://www.w3schools.com/html/tryit.asp?filename=tryhtml_radio and https://www.laits.utexas.edu/hebrew/drupal/themes/hebrewgrid/personal/toolbox/forms/quiz13.html
 
echo "HTTP/1.0 200 OK"
echo "Content-type: text/html"
echo

cat << EOF
<body>
<form action="mailto:kstaple1@nd.edu">
1. The best Harry Potter character is:<BR>
<input type="radio" name="1. The best Harry Potter character is" value="Hermione">Hermione<BR>
<input type="radio" name="1. The best Harry Potter character is" value="Snape">Snape<BR>
<input type="radio" name="1. The best Harry Potter character is"" value="Voldemort">Voldemort<BR>

2. My favorite book is the _th one:<BR>
<input type="radio" name="2. My favorite book is the _th one:" value="1">1<BR>
<input type="radio" name="2. My favorite book is the _th one:" value="2">2<BR>
<input type="radio" name="2. My favorite book is the _th one:" value="3">3<BR>
<input type="radio" name="2. My favorite book is the _th one:" value="4">4<BR>
<input type="radio" name="2. My favorite book is the _th one:" value="5">5<BR>
<input type="radio" name="2. My favorite book is the _th one:" value="6">6<BR>
<input type="radio" name="2. My favorite book is the _th one:" value="7">7<BR>


3.The best wizarding place is:<BR>
<input type="radio" name="place" value="Hogwarts">Hogwarts<BR>
<input type="radio" name="place" value="Diagon Alley">Diagon Alley<BR>
<input type="radio" name="place" value="The Weasleys' house">The Weasleys' house<BR>


<input type="submit" value="Send Form">
</form>
</body>

EOF
