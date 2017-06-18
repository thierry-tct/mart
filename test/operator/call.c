void callf(int a, float *b)
{
    return;
}
void callg(int x, float *y)
{
    callf(x,y);
}
void dirname(int a, float *b)
{
    return;
}
void basename(int x, float *y)
{
    dirname(x,y);
}
void xf(int j, float ff, int k, double *y, long *z)
{

}
int main ()
{
    int i;
    float j;
    callg(i,&j);
    basename(i,&j);
    xf(4, 10.5, i, (double *)0, (long *)0);
    return 0;
}
