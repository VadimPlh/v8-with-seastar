function user_script(obj) {
    let array = new Int32Array(obj);
    for (let i = 0; i < array[0]; i++) {
        array[1] += array[i + 2];
    }
}