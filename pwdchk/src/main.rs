mod account;

use account::{group, Account};
use clap::{Args, Parser, Subcommand};

#[derive(Parser)]
#[clap(version, author, about)]
struct AppArgs {
    #[clap(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Check duplicate passwords from command line
    Group(GroupArgs),
}

#[derive(Args)]
struct GroupArgs {
    #[clap(required = true)]
    /// Account to check
    account: Vec<Account>,
}

fn main() {
    // Check command line
    let args = AppArgs::parse();
    match args.command {
        Command::Group(args) => {
            let same_password_groups = group(args.account);
            for entry in same_password_groups {
                println!("Password {0} used by {1}", entry.0, entry.1.join(", "));
            }
        }
    }
}
