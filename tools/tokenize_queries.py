from argparse import ArgumentParser
from transformers.models.bert.tokenization_bert import BasicTokenizer
def main():
    parser = ArgumentParser(description='Tokenize queries.')
    parser.add_argument('--input', dest='input', required=True)
    parser.add_argument('--output', dest='output', required=True)
    args = parser.parse_args()
    basic_tokenizer = BasicTokenizer(do_lower_case=True, strip_accents=True)
    with open(args.input) as input_file, open(args.output, "w+") as output_file:
        for line in input_file:
            id, query = line.split("\t")
            tokens = basic_tokenizer.tokenize(query.strip())
            output_file.write("{}\t{}\n".format(id, " ".join(tokens)))
if __name__ == "__main__":
    main()
