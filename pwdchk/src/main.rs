mod account;

use account::{group, Account, NoColon};
use std::env;
use std::str::FromStr;

fn main() -> Result<(), NoColon> {
    // getting rid of the first argument (path of the executable)
    let arguments = env::args().skip(1);

    let accounts = arguments
        .map(|s| Account::from_str(&s))
        .collect::<Result<Vec<_>, _>>()?;

    let same_password_groups = group(accounts);
    for entry in same_password_groups {
        println!("Password {0} used by {1}", entry.0, entry.1.join(", "));
    }
    Ok(())
}
