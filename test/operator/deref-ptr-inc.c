int main ()
{
    int i[3], a, *pi, *pp;
    pi = pp = i;
    a = *(++pi) + (*pp)--;
    return a;
}
