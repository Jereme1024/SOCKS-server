Project #1: Remote Access System (ras).

In this homework, you are asked to design rsh-like access systems,
called remote access systems, including both
client and server.  In this system, the server designates a directory,
say ras/; then, clients can only run executable programs inside the
directory, ras/.

The following is a scenario of using the system.

csh> telnet myserver.nctu.edu.tw 7000 # the server port number
**************************************************************
** Welcome to the information server, myserver.nctu.edu.tw. **
**************************************************************
** You are in the directory, /home/studentA/ras/.
** This directory will be under "/", in this system.
** This directory includes the following executable programs.
**
**	bin/
**	test.html	(test file)
**
** The directory bin/ includes:
**	cat
**	ls
**	removetag		(Remove HTML tags.)
**	removetag0		(Remove HTML tags with error message.)
**	number  		(Add a number in each line.)
**
** In addition, the following two commands are supported by ras.
**	setenv
**	printenv
**
% printenv PATH                       # Initial PATH is bin/ and ./
PATH=bin:.
% setenv PATH bin                     # Set to bin/ only
% printenv PATH
PATH=bin
% ls
bin/		test.html
% ls bin
ls		cat		removetag     removetag0    number
% cat test.html > test1.txt
% cat test1.txt
<!test.html>
<TITLE>Test<TITLE>
<BODY>This is a <b>test</b> program
for ras.
</BODY>
% removetag test.html

Test
This is a test program
for ras.

% removetag test.html > test2.txt
% cat test2.txt

Test
This is a test programsfor ras.

% removetag0 test.html
Error: illegal tag "!test.html"

Test
This is a test program
for ras.

% removetag0 test.html > test2.txt
Error: illegal tag "!test.html"
% cat test2.txt

Test
This is a test program
for ras.

% removetag test.html | number
  1
  2 Test
  3 This is a test program
  4 for ras.
  5
% removetag test.html |1 number > test3.txt		# |1 does the same thing as normal pipe
% cat test3.txt
  1
  2 Test
  3 This is a test programs  4 for ras.
  5
% removetag test.html |3 removetag test.html | number |1 number		# |3 skips two processes.
  1
  2 Test
  3 This is a test program
  4 for ras.
  5
  6	1
  7	2 Test
  8	3 This is a test program
  9	4 for ras.
 10	5
% ls |2 ls | cat		# in this case, two ls output to the same pipe, the output of second ls will append to the output of first ls.
  bin/                
  test.html
  test1.txt
  test2.txt
  bin/
  test.html
  test1.txt
  test2.txt
% ls |2 removetag test.html		# ls pipe to next command

  Test
  This is a test program
  for ras.

% cat							# ls pipe to this command
  bin/
  test.html
  test1.txt
  test2.txt
% ls |2							# only pipe to second next legal process, doesn’t output
% asdasdas						# illegal command will not be counted
  Unknown command: [asdasdas].
% removetag test.html | cat		# cat is second next legal process of ls
  bin/
  test.html
  test1.txt
  test2.txt

  Test
  This is a test program
  for ras.

% date
  Unknown Command: [date].
# Let TA do this "cp /bin/date bin"  in your csh directory
% date
  Wed Oct  1 00:41:50 CST 2003
% exit
csh>
