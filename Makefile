all:
	g++ main.cpp
	g++ main_c.cpp -o b.out

clean:
	rm a.out
