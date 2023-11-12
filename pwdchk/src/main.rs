mod account;

use account::Account;

fn main() {
    println!(
        "{:?}",
        Account::from_string("johndoe:super:complex:password")
    );
}
