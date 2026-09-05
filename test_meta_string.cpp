struct init_from {
    const char* field_name;
};

struct A {
    [[=init_from{"hello"}]]
    int x;
};

int main() {
    return 0;
}
