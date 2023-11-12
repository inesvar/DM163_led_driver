mod account;

use account::Account;

fn main() {
    let account = Account::new("johndoe", "super:complex:password");
    println!("{account:?}");
}
