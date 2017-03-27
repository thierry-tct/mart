void callf(int a, float *b)
{
    return;
}
void callg(int x, float *y)
{
    callf(x,y);
}
int main ()
{
    int i;
    float j;
    callg(i,&j);
    return 0;
}
