gcd(a, b) {
    if (b == 0) {
        return a;
    } else {
        return gcd(b, a % b);
    }
}

main(argc, argv) {
    if(argc != 3) {
        // We could call puts function here to print an error message
        // However this would have to be done with bytes since the language has no strings / char types
        return -1;
    }

    register x = atol(argv[1]);
    register y = atol(argv[2]);
    return gcd(x, y);
}