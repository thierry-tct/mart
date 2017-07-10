int main ()
{
    int i[4][3], (*pi)[3], *q;
    pi=&i[0]; pi++; pi=pi-1;
    q = *(pi-1); q=*(pi)+1;
    return *q;
}
