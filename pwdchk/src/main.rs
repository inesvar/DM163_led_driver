mod account;
mod error;
mod hibp;

use account::{group, Account};
use clap::{ArgGroup, Args, Parser, Subcommand};
use eyre::Result;
use hibp::check_accounts;
use std::path::PathBuf;

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
    Hibp(HibpArgs),
}

#[derive(Args)]
#[clap(group(
    ArgGroup::new("input")
        .required(true)
        .args(&["file", "account"]),
))]
struct GroupArgs {
    /// Account to check
    account: Vec<Account>,
    #[clap(short, long)]
    /// Load passwords from a file
    file: Option<PathBuf>,
}

#[derive(Args)]
struct HibpArgs {
    #[clap(short, long, required = true)]
    /// Load passwords from a file
    file: PathBuf,
}

fn main() -> Result<()> {
    let _ = color_eyre::install();
    // Check command line
    let args = AppArgs::parse();
    match args.command {
        Command::Group(GroupArgs {
            account: _,
            file: Some(filename),
        }) => {
            let accounts = Account::from_file(filename.as_path())?;
            let same_password_groups = group(&accounts);
            for entry in same_password_groups {
                println!("Password {0} used by {1}", entry.0, entry.1.join(", "));
            }
        }
        Command::Group(args) => {
            let same_password_groups = group(&args.account);
            for entry in same_password_groups {
                println!("Password {0} used by {1}", entry.0, entry.1.join(", "));
            }
        }
        Command::Hibp(HibpArgs { file: filename }) => {
            let accounts = Account::from_file(filename.as_path())?;
            let pawned_accounts = check_accounts(&accounts)?;
            println!("{:#?}", pawned_accounts);
        }
    }
    Ok(())
}
