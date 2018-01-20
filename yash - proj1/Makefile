make: yash.c child.c childchild.c
	gcc yash.c child.c childchild.c jobCtrl.c -o yash
debug: yash.c child.c childchild.c
	gcc -g yash.c child.c childchild.c jobCtrl.c 
	gdbtui -q ./a.out
clean:
	rm a.out yash file*.txt
