int main ()
{
    int i[3], a, *pi, *pp;
    pi = pp = i;
    a = *(pi+78);
    a = a-(*pi);
    return (*pp)-a;
}
