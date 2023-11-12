mod account;

use account::{Account, NoColon};
use std::env;
use std::str::FromStr;

fn main() -> Result<(), NoColon> {
    // getting rid of the first argument (path of the executable)
    let arguments = env::args().skip(1);

    let accounts = arguments
        .map(|s| Account::from_str(&s))
        .collect::<Result<Vec<_>, _>>()?;
    println!("{accounts:#?}");
    Ok(())
}
