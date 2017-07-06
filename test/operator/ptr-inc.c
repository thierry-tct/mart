int main ()
{
    int i[3], *pi, *pp;
    pi=i+1;
    
    pp = ++pi;
    return *pp;
}
